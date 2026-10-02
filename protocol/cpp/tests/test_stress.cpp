// Stress test (Phase 1 brief §5.4): 1,000,000 TEST_PATTERN records through EACH ring, both rings
// at once, producer and consumer on separate threads, over a real named mapping. Every record is
// checked for order and content. A watchdog fails the test instead of hanging on a deadlock.
#include "test.h"

#include "redcraft/codec.h"
#include "redcraft/mapping.h"
#include "redcraft/ring.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

using namespace redcraft;
using namespace redcraft::proto;

namespace
{
	constexpr std::uint64_t kStressMessages = 1000000;
	constexpr auto          kWatchdog = std::chrono::seconds(120);
	constexpr std::uint32_t kStressSession = 42;

	struct Stream
	{
		std::atomic<std::uint64_t> sent{ 0 };
		std::atomic<std::uint64_t> received{ 0 };
		std::atomic<std::uint64_t> errors{ 0 };
		std::atomic<std::uint64_t> fullRetries{ 0 };
	};

	void Produce(const RingLocation& a_ring, Stream& a_stream, const std::atomic<bool>& a_abort)
	{
		RingProducer producer;
		producer.Bind(a_ring.control, a_ring.data, a_ring.dataBytes);
		RingStats    stats;
		std::uint8_t buf[kTestPatternFixedBytes + kTestPatternMaxFill];
		for (std::uint64_t i = 0; i < kStressMessages && !a_abort;) {
			const std::uint32_t n = codec::BuildTestPattern(i, buf);
			const auto          r = producer.Push(kMsgTestPattern, 1, kStressSession, static_cast<std::uint32_t>(i + 1), buf, n, stats);
			if (r == PushResult::kOk) {
				++i;
				a_stream.sent.store(i, std::memory_order_relaxed);
			} else {
				a_stream.fullRetries.fetch_add(1, std::memory_order_relaxed);
				std::this_thread::yield();
			}
		}
	}

	void Consume(const RingLocation& a_ring, Stream& a_stream, const std::atomic<bool>& a_abort)
	{
		RingConsumer consumer;
		consumer.Bind(a_ring.control, a_ring.data, a_ring.dataBytes);
		RingStats     stats;
		std::uint32_t session = kStressSession;
		std::uint64_t expected = 0;
		while (expected < kStressMessages && !a_abort) {
			const auto st = consumer.Drain(&session, kMaxDrainBytesPerTick, stats, [&](const RecordHeader& h, const std::uint8_t* p) {
				std::uint64_t idx = 0;
				if (h.type != kMsgTestPattern || !codec::CheckTestPattern(p, h.payloadBytes, idx) || idx != expected ||
					h.seq != static_cast<std::uint32_t>(expected + 1)) {
					a_stream.errors.fetch_add(1, std::memory_order_relaxed);
				}
				++expected;
			});
			if (st != DrainStatus::kOk) {
				a_stream.errors.fetch_add(1, std::memory_order_relaxed);
			}
			a_stream.received.store(expected, std::memory_order_relaxed);
			if (consumer.Pending() == 0) {
				std::this_thread::yield();
			}
		}
		if (stats.stale || stats.corrupt) {
			a_stream.errors.fetch_add(stats.stale + stats.corrupt, std::memory_order_relaxed);
		}
	}
}

TEST_CASE("stress: 1,000,000 records through each ring at once, no corruption, no deadlock")
{
	const std::wstring name = L"Local\\RedCraft_Test_stress_" + std::to_wstring(::GetCurrentProcessId());
	SharedMapping      mapping;
	char               err[160] = {};
	REQUIRE(mapping.CreateOrOpen(name.c_str(), Role::kHost, err, sizeof(err)));
	REQUIRE(mapping.PollReady(err, sizeof(err)) == ReadyResult::kReady);
	const Layout& layout = mapping.GetLayout();

	Stream            h2m, m2h;
	std::atomic<bool> abort{ false };
	const auto        start = std::chrono::steady_clock::now();
	std::thread       t1(Produce, std::cref(layout.hostToMc), std::ref(h2m), std::cref(abort));
	std::thread       t2(Consume, std::cref(layout.hostToMc), std::ref(h2m), std::cref(abort));
	std::thread       t3(Produce, std::cref(layout.mcToHost), std::ref(m2h), std::cref(abort));
	std::thread       t4(Consume, std::cref(layout.mcToHost), std::ref(m2h), std::cref(abort));

	while (h2m.received < kStressMessages || m2h.received < kStressMessages) {
		if (std::chrono::steady_clock::now() - start > kWatchdog) {
			abort = true;
			test::Fail(__FILE__, __LINE__, "watchdog: stress test did not finish (deadlock?)");
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
	t1.join();
	t2.join();
	t3.join();
	t4.join();
	const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
	std::printf("    host->mc: %llu sent, %llu received, %llu errors, %llu full-retries\n", static_cast<unsigned long long>(h2m.sent.load()),
		static_cast<unsigned long long>(h2m.received.load()), static_cast<unsigned long long>(h2m.errors.load()),
		static_cast<unsigned long long>(h2m.fullRetries.load()));
	std::printf("    mc->host: %llu sent, %llu received, %llu errors, %llu full-retries\n", static_cast<unsigned long long>(m2h.sent.load()),
		static_cast<unsigned long long>(m2h.received.load()), static_cast<unsigned long long>(m2h.errors.load()),
		static_cast<unsigned long long>(m2h.fullRetries.load()));
	std::printf("    %lld ms total\n", static_cast<long long>(ms));
	CHECK_EQ(h2m.received.load(), kStressMessages);
	CHECK_EQ(m2h.received.load(), kStressMessages);
	CHECK_EQ(h2m.errors.load(), std::uint64_t(0));
	CHECK_EQ(m2h.errors.load(), std::uint64_t(0));
}
