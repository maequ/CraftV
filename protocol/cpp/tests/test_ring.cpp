// Ring tests on a private buffer (PROTOCOL.md §4): wrap-around with PAD, full ring, stale
// sessions, corruption handling, and that the consumer never reads outside the data area.
#include "test.h"

#include "redcraft/codec.h"
#include "redcraft/ring.h"

#include <cstring>
#include <memory>
#include <vector>

using namespace redcraft;
using namespace redcraft::proto;

namespace
{
	constexpr std::uint64_t kTestDataBytes = 4096;

	struct TestRing
	{
		std::unique_ptr<RingControl>  control = std::make_unique<RingControl>();
		std::vector<std::uint8_t>     data = std::vector<std::uint8_t>(kTestDataBytes, 0xEE);
		std::uint32_t                 producerSession = 1;
		RingProducer                  producer;
		RingConsumer                  consumer;
		RingStats                     txStats, rxStats;

		TestRing()
		{
			std::memset(control.get(), 0, sizeof(RingControl));
			control->dataBytes = kTestDataBytes;
			producer.Bind(control.get(), data.data(), kTestDataBytes);
			consumer.Bind(control.get(), data.data(), kTestDataBytes);
		}

		PushResult Push(std::uint16_t a_type, std::uint32_t a_bytes, std::uint8_t a_fill, std::uint32_t a_seq = 1)
		{
			std::vector<std::uint8_t> payload(a_bytes, a_fill);
			return producer.Push(a_type, 1, producerSession, a_seq, payload.data(), a_bytes, txStats);
		}

		// Returns the payload sizes seen, in order.
		std::vector<std::uint32_t> DrainSizes(DrainStatus* a_status = nullptr, std::uint64_t a_max = ~0ull)
		{
			std::vector<std::uint32_t> sizes;
			const auto status = consumer.Drain(&producerSession, a_max, rxStats, [&](const RecordHeader& h, const std::uint8_t*) {
				sizes.push_back(h.payloadBytes);
			});
			if (a_status) {
				*a_status = status;
			}
			return sizes;
		}
	};
}

TEST_CASE("ring: records round-trip in order with payload intact")
{
	TestRing r;
	CHECK(r.Push(kMsgLog, 10, 0x11) == PushResult::kOk);
	CHECK(r.Push(kMsgLog, 0, 0x00) == PushResult::kOk);
	CHECK(r.Push(kMsgLog, 33, 0x22) == PushResult::kOk);
	std::vector<std::vector<std::uint8_t>> seen;
	r.consumer.Drain(&r.producerSession, ~0ull, r.rxStats, [&](const RecordHeader& h, const std::uint8_t* p) {
		seen.emplace_back(p, p + h.payloadBytes);
	});
	REQUIRE(seen.size() == 3);
	CHECK(seen[0] == std::vector<std::uint8_t>(10, 0x11));
	CHECK(seen[1].empty());
	CHECK(seen[2] == std::vector<std::uint8_t>(33, 0x22));
	CHECK_EQ(r.control->head, r.control->tail);
	CHECK_EQ(r.control->head, RecordBytes(10) + RecordBytes(0) + RecordBytes(33));
}

TEST_CASE("ring: wrap-around writes a PAD and never straddles the end")
{
	TestRing r;
	// Fill to 32 bytes short of the end, consume, then push a record that doesn't fit before the end.
	const std::uint32_t big = static_cast<std::uint32_t>(kTestDataBytes - 32 - kRecordHeaderBytes);
	CHECK(r.Push(kMsgLog, big, 0x33) == PushResult::kOk);
	CHECK(r.DrainSizes().size() == 1);
	CHECK(r.Push(kMsgLog, 100, 0x44) == PushResult::kOk);  // needs 128 > 32 left: PAD + wrap
	CHECK_EQ(r.control->head, kTestDataBytes + RecordBytes(100));
	RecordHeader pad;
	std::memcpy(&pad, r.data.data() + kTestDataBytes - 32, sizeof(pad));
	CHECK_EQ(pad.type, std::uint16_t(kMsgPad));
	const auto sizes = r.DrainSizes();
	REQUIRE(sizes.size() == 1);
	CHECK_EQ(sizes[0], std::uint32_t(100));
	CHECK_EQ(r.rxStats.corrupt, std::uint64_t(0));
}

TEST_CASE("ring: a full ring drops instead of blocking, then recovers")
{
	TestRing r;
	int pushed = 0;
	while (r.Push(kMsgLog, 240, 0x55) == PushResult::kOk) {
		++pushed;
	}
	CHECK(pushed > 0);
	CHECK_EQ(r.txStats.droppedFull, std::uint64_t(1));
	CHECK(r.producer.Backlog() <= kTestDataBytes);
	CHECK_EQ(r.DrainSizes().size(), static_cast<std::size_t>(pushed));
	CHECK(r.Push(kMsgLog, 240, 0x55) == PushResult::kOk);
	CHECK(r.Push(kMsgLog, kMaxPayload + 1, 0) == PushResult::kTooLarge);
}

TEST_CASE("ring: many wraps with varying sizes keep every record intact")
{
	TestRing      r;
	std::uint64_t sent = 0, received = 0, bad = 0;
	std::uint8_t  buf[kTestPatternFixedBytes + kTestPatternMaxFill];
	for (int round = 0; round < 2000; ++round) {
		for (int k = 0; k < 3; ++k) {
			const std::uint32_t n = codec::BuildTestPattern(sent, buf);
			if (r.producer.Push(kMsgTestPattern, 1, r.producerSession, 1, buf, n, r.txStats) == PushResult::kOk) {
				++sent;
			}
		}
		r.consumer.Drain(&r.producerSession, 700, r.rxStats, [&](const RecordHeader& h, const std::uint8_t* p) {
			std::uint64_t idx = 0;
			if (!codec::CheckTestPattern(p, h.payloadBytes, idx) || idx != received) {
				++bad;
			}
			++received;
		});
	}
	r.DrainSizes();  // finish
	CHECK(sent > 1000);
	CHECK_EQ(bad, std::uint64_t(0));
	CHECK_EQ(r.rxStats.corrupt, std::uint64_t(0));
	CHECK(r.control->head > 50 * kTestDataBytes);  // wrapped many times
}

TEST_CASE("ring: records from an old producer session are skipped as stale")
{
	TestRing r;
	r.Push(kMsgLog, 8, 1);
	r.Push(kMsgLog, 8, 1);
	r.producerSession = 2;  // producer restarted; the two records above are from session 1
	r.Push(kMsgLog, 16, 2);
	const auto sizes = r.DrainSizes();
	REQUIRE(sizes.size() == 1);
	CHECK_EQ(sizes[0], std::uint32_t(16));
	CHECK_EQ(r.rxStats.stale, std::uint64_t(2));
}

TEST_CASE("ring: corrupt framing drops the backlog and stays in bounds")
{
	{
		TestRing r;
		r.Push(kMsgLog, 8, 1);
		RecordHeader h;
		std::memcpy(&h, r.data.data(), sizeof(h));
		h.payloadBytes = 0x7FFFFFFF;  // absurd size
		std::memcpy(r.data.data(), &h, sizeof(h));
		DrainStatus st;
		CHECK(r.DrainSizes(&st).empty());
		CHECK(st == DrainStatus::kCorrupt);
		CHECK_EQ(r.rxStats.corrupt, std::uint64_t(1));
		CHECK_EQ(r.control->tail, r.control->head);
		CHECK(r.Push(kMsgLog, 8, 1) == PushResult::kOk);  // usable again
		CHECK_EQ(r.DrainSizes().size(), std::size_t(1));
	}
	{
		TestRing r;
		r.control->head = kTestDataBytes * 5;  // claims more than the ring holds
		DrainStatus st;
		r.DrainSizes(&st);
		CHECK(st == DrainStatus::kCorrupt);
	}
	{
		TestRing r;
		r.control->head = 24;  // not 16-aligned
		DrainStatus st;
		r.DrainSizes(&st);
		CHECK(st == DrainStatus::kCorrupt);
	}
	{
		TestRing r;
		r.Push(kMsgLog, 64, 1);
		r.control->head = 32;  // record claims 80 bytes but head says 32
		DrainStatus st;
		r.DrainSizes(&st);
		CHECK(st == DrainStatus::kCorrupt);
	}
	{
		// A consumer that wrote a tail beyond head: the producer treats the ring as full.
		TestRing r;
		r.control->tail = 1 << 20;
		CHECK(r.Push(kMsgLog, 8, 1) == PushResult::kFull);
	}
}

TEST_CASE("ring: the per-call byte budget is honoured and the rest is kept")
{
	TestRing r;
	for (int i = 0; i < 10; ++i) {
		r.Push(kMsgLog, 16, 1);  // 32-byte records
	}
	CHECK_EQ(r.DrainSizes(nullptr, 64).size(), std::size_t(2));
	CHECK_EQ(r.DrainSizes().size(), std::size_t(8));
}

TEST_CASE("ring: DiscardBacklog empties the ring")
{
	TestRing r;
	r.Push(kMsgLog, 16, 1);
	r.Push(kMsgLog, 16, 1);
	r.consumer.DiscardBacklog();
	CHECK(r.DrainSizes().empty());
	CHECK_EQ(r.consumer.Pending(), std::uint64_t(0));
}
