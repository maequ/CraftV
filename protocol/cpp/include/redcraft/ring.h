// Single-producer / single-consumer byte ring over shared memory (PROTOCOL.md §4, §6).
//
// Algorithm from SkyCraft's collision/render rings (MIT, chasmlol): running u64 head/tail counts on
// separate cache lines, records never straddle the end (a PAD record skips to the start), unknown
// types are skipped by length. RedCraft adds 16-byte records with a session tag and full validation of
// everything the peer wrote. Neither side ever blocks; no heap allocation.
#pragma once

#include "redcraft/protocol.h"

#include <atomic>
#include <cstring>

namespace redcraft
{
	struct RingStats
	{
		std::uint64_t produced = 0;
		std::uint64_t producedBytes = 0;
		std::uint64_t droppedFull = 0;
		std::uint64_t consumed = 0;
		std::uint64_t consumedBytes = 0;
		std::uint64_t stale = 0;      // records left by a previous incarnation of the producer
		std::uint64_t unknown = 0;    // unknown type, skipped by length
		std::uint64_t malformed = 0;  // known type, bad size / values / direction
		std::uint64_t corrupt = 0;    // ring framing broken; backlog dropped
	};

	enum class PushResult
	{
		kOk,
		kFull,
		kTooLarge,
		kUnbound,
	};

	enum class DrainStatus
	{
		kOk,
		kCorrupt,
		kUnbound,
	};

	namespace detail
	{
		template <class T>
		std::atomic_ref<T> Atomic(T& a_value)
		{
			return std::atomic_ref<T>(a_value);
		}
	}

	// The producing end of one ring. Exactly one thread in one process may use it. (PROTOCOL.md §4.2 "Produce")
	class RingProducer
	{
	public:
		void Bind(proto::RingControl* a_control, std::uint8_t* a_data, std::uint64_t a_dataBytes)
		{
			control_ = a_control;
			data_ = a_data;
			dataBytes_ = a_dataBytes;
			// Continue from the current head (§5.2 step 3); we are its only writer from now on.
			head_ = detail::Atomic(control_->head).load(std::memory_order_acquire);
		}

		void Unbind() { control_ = nullptr; }
		bool Bound() const { return control_ != nullptr; }

		PushResult Push(std::uint16_t a_type, std::uint16_t a_typeVersion, std::uint32_t a_session, std::uint32_t a_seq,
			const void* a_payload, std::uint32_t a_payloadBytes, RingStats& a_stats)
		{
			if (!control_) {
				return PushResult::kUnbound;
			}
			if (a_payloadBytes > proto::kMaxPayload) {
				return PushResult::kTooLarge;
			}
			const std::uint64_t size = proto::RecordBytes(a_payloadBytes);
			std::uint64_t       pos = head_ & (dataBytes_ - 1);
			const std::uint64_t pad = (pos + size > dataBytes_) ? dataBytes_ - pos : 0;
			const std::uint64_t tail = detail::Atomic(control_->tail).load(std::memory_order_acquire);
			const std::uint64_t used = head_ - tail;
			// A consumer that wrote nonsense (used > capacity) reads as "full" until it resyncs (§4.3).
			if (used > dataBytes_ || dataBytes_ - used < size + pad) {
				++a_stats.droppedFull;
				return PushResult::kFull;
			}
			if (pad) {
				const proto::RecordHeader padHeader{ proto::kMsgPad, 0, 0, a_session, 0 };
				std::memcpy(data_ + pos, &padHeader, sizeof(padHeader));
				head_ += pad;
				pos = 0;
			}
			const proto::RecordHeader header{ a_type, a_typeVersion, a_payloadBytes, a_session, a_seq };
			std::uint8_t*             at = data_ + pos;
			std::memcpy(at, &header, sizeof(header));
			if (a_payloadBytes) {
				std::memcpy(at + proto::kRecordHeaderBytes, a_payload, a_payloadBytes);
			}
			std::memset(at + proto::kRecordHeaderBytes + a_payloadBytes, 0, size - proto::kRecordHeaderBytes - a_payloadBytes);
			head_ += size;
			detail::Atomic(control_->head).store(head_, std::memory_order_release);
			++a_stats.produced;
			a_stats.producedBytes += size;
			return PushResult::kOk;
		}

		std::uint64_t Backlog() const
		{
			return control_ ? head_ - detail::Atomic(control_->tail).load(std::memory_order_acquire) : 0;
		}

	private:
		proto::RingControl* control_ = nullptr;
		std::uint8_t*       data_ = nullptr;
		std::uint64_t       dataBytes_ = 0;
		std::uint64_t       head_ = 0;
	};

	// The consuming end of one ring. Exactly one thread in one process may use it. (PROTOCOL.md §4.2 "Consume")
	class RingConsumer
	{
	public:
		void Bind(proto::RingControl* a_control, const std::uint8_t* a_data, std::uint64_t a_dataBytes)
		{
			control_ = a_control;
			data_ = a_data;
			dataBytes_ = a_dataBytes;
		}

		void Unbind() { control_ = nullptr; }
		bool Bound() const { return control_ != nullptr; }

		// Drops everything pending (§5.2 step 2, §4.3). Safe: we are the only writer of tail.
		void DiscardBacklog()
		{
			if (control_) {
				const auto head = detail::Atomic(control_->head).load(std::memory_order_acquire);
				detail::Atomic(control_->tail).store(head, std::memory_order_release);
			}
		}

		// Calls a_onRecord(const RecordHeader&, const std::uint8_t* payload) for each valid record of
		// the producer's current session, at most about a_maxBytes per call. a_producerSession is the
		// producer's side-block session field; it is read (acquire) after head, as §4.2 requires.
		template <class F>
		DrainStatus Drain(const std::uint32_t* a_producerSession, std::uint64_t a_maxBytes, RingStats& a_stats, F&& a_onRecord)
		{
			if (!control_) {
				return DrainStatus::kUnbound;
			}
			const std::uint64_t head = detail::Atomic(control_->head).load(std::memory_order_acquire);
			const std::uint32_t session = detail::Atomic(*const_cast<std::uint32_t*>(a_producerSession)).load(std::memory_order_acquire);
			std::uint64_t       tail = detail::Atomic(control_->tail).load(std::memory_order_relaxed);
			const std::uint64_t pending = head - tail;
			if (pending > dataBytes_ || (pending % proto::kRecordAlign) != 0 || (tail % proto::kRecordAlign) != 0) {
				return Corrupt(head, a_stats);
			}
			std::uint64_t done = 0;
			while (tail < head && done < a_maxBytes) {
				const std::uint64_t pos = tail & (dataBytes_ - 1);
				const std::uint64_t toEnd = dataBytes_ - pos;
				if (toEnd < proto::kRecordHeaderBytes) {
					tail += toEnd;
					continue;
				}
				proto::RecordHeader header;
				std::memcpy(&header, data_ + pos, sizeof(header));
				if (header.type == proto::kMsgPad) {
					if (toEnd > head - tail) {
						return Corrupt(head, a_stats);
					}
					tail += toEnd;
					continue;
				}
				if (header.payloadBytes > proto::kMaxPayload) {
					return Corrupt(head, a_stats);
				}
				const std::uint64_t size = proto::RecordBytes(header.payloadBytes);
				if (size > toEnd || size > head - tail) {
					return Corrupt(head, a_stats);
				}
				if (header.session != session) {
					++a_stats.stale;
				} else {
					++a_stats.consumed;
					a_stats.consumedBytes += size;
					a_onRecord(static_cast<const proto::RecordHeader&>(header), data_ + pos + proto::kRecordHeaderBytes);
				}
				tail += size;
				done += size;
			}
			detail::Atomic(control_->tail).store(tail, std::memory_order_release);
			return DrainStatus::kOk;
		}

		std::uint64_t Pending() const
		{
			if (!control_) {
				return 0;
			}
			return detail::Atomic(control_->head).load(std::memory_order_acquire) - detail::Atomic(control_->tail).load(std::memory_order_relaxed);
		}

	private:
		DrainStatus Corrupt(std::uint64_t a_head, RingStats& a_stats)
		{
			++a_stats.corrupt;
			detail::Atomic(control_->tail).store(a_head, std::memory_order_release);
			return DrainStatus::kCorrupt;
		}

		proto::RingControl* control_ = nullptr;
		const std::uint8_t* data_ = nullptr;
		std::uint64_t       dataBytes_ = 0;
	};
}
