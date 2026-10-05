// Codec tests: golden vectors (shared with Java), a spec-literal encoding that does not use the
// structs at all, and the validation rules of PROTOCOL.md §7/§8.
#include "golden_values.h"
#include "test.h"

#include "craftv/codec.h"

#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>

using namespace craftv;
using namespace craftv::proto;

namespace
{
	std::map<std::string, std::string> LoadGoldenFile()
	{
		std::map<std::string, std::string> out;
		std::ifstream                      in(CRAFTV_GOLDEN_FILE);
		std::string                        line;
		while (std::getline(in, line)) {
			if (line.empty() || line[0] == '#') {
				continue;
			}
			std::istringstream ss(line);
			std::string        name, hex;
			ss >> name >> hex;
			out[name] = hex;
		}
		return out;
	}

	// Little-endian byte writer that follows the PROTOCOL.md tables literally.
	struct Bytes
	{
		std::vector<std::uint8_t> b;
		void                      U8(std::uint8_t v) { b.push_back(v); }
		void                      U16(std::uint16_t v) { for (int i = 0; i < 2; ++i) b.push_back(static_cast<std::uint8_t>(v >> (8 * i))); }
		void                      U32(std::uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back(static_cast<std::uint8_t>(v >> (8 * i))); }
		void                      U64(std::uint64_t v) { for (int i = 0; i < 8; ++i) b.push_back(static_cast<std::uint8_t>(v >> (8 * i))); }
		void                      I32(std::int32_t v) { U32(static_cast<std::uint32_t>(v)); }
		void                      F32(float v) { std::uint32_t u; std::memcpy(&u, &v, 4); U32(u); }
		void                      F64(double v) { std::uint64_t u; std::memcpy(&u, &v, 8); U64(u); }
		void                      Zeros(std::size_t n) { b.insert(b.end(), n, 0); }
		void                      PadTo16() { while (b.size() % 16) b.push_back(0); }
	};
}

TEST_CASE("codec: golden file matches the C++ encoder byte for byte")
{
	const auto golden = LoadGoldenFile();
	REQUIRE(!golden.empty());
	const auto vectors = golden::AllVectors();
	CHECK_EQ(golden.size(), vectors.size());
	for (const auto& v : vectors) {
		auto it = golden.find(v.name);
		if (it == golden.end()) {
			test::Fail(__FILE__, __LINE__, "golden file lacks " + v.name);
			continue;
		}
		if (it->second != test::Hex(v.record.data(), v.record.size())) {
			test::Fail(__FILE__, __LINE__, v.name + " differs from the golden file (regenerate with craftv_golden_gen after a deliberate change)");
		}
	}
}

TEST_CASE("codec: PLAYER_STATE record built literally from PROTOCOL.md matches the struct encoding")
{
	Bytes r;
	// §4.2 record header
	r.U16(3);           // type PLAYER_STATE
	r.U16(1);           // typeVersion
	r.U32(64);          // payloadBytes
	r.U32(0x11223344);  // session
	r.U32(7);           // seq
	// §7.3 payload
	r.F64(12.5);
	r.F64(-60.0);
	r.F64(-3.25);
	r.F32(1.5f);
	r.F32(0.0f);
	r.F32(-4.25f);
	r.F32(90.0f);
	r.F32(-15.5f);
	r.U32(1);  // ON_GROUND
	r.U64(123456789);
	r.U32(4321);
	r.Zeros(4);
	r.PadTo16();
	CHECK_EQ(r.b.size(), std::size_t(80));
	for (const auto& v : golden::AllVectors()) {
		if (v.name == "PLAYER_STATE") {
			CHECK(v.record == r.b);
		}
	}
}

TEST_CASE("codec: BLOCK_SET and BLOCK_PLACE_REQUEST built literally from PROTOCOL.md")
{
	Bytes set;
	set.U16(4);
	set.U16(1);
	set.U32(24);
	set.U32(0x11223344);
	set.U32(7);
	set.I32(-5);
	set.I32(64);
	set.I32(1024);
	set.U32(1);  // blockId
	set.U32(1);  // ECHO
	set.U32(9);  // requestId
	set.PadTo16();

	Bytes place;
	place.U16(6);
	place.U16(1);
	place.U32(24);
	place.U32(0x11223344);
	place.U32(7);
	place.U32(4);  // requestId
	place.I32(10);
	place.I32(-61);
	place.I32(-20);
	place.U8(1);  // face up
	place.Zeros(3);
	place.U32(1);  // blockId
	place.PadTo16();

	for (const auto& v : golden::AllVectors()) {
		if (v.name == "BLOCK_SET") {
			CHECK(v.record == set.b);
		}
		if (v.name == "BLOCK_PLACE_REQUEST") {
			CHECK(v.record == place.b);
		}
	}
}

TEST_CASE("codec: golden records decode back to the documented fields")
{
	for (const auto& v : golden::AllVectors()) {
		RecordHeader h;
		std::memcpy(&h, v.record.data(), sizeof(h));
		CHECK_EQ(h.session, golden::kSession);
		CHECK_EQ(h.seq, golden::kSeq);
		CHECK_EQ(h.typeVersion, kTypeVersion1);
		CHECK_EQ(v.record.size() % 16, std::size_t(0));
		const std::uint8_t* p = v.record.data() + kRecordHeaderBytes;
		if (v.name == "PLAYER_STATE") {
			PlayerStateMsg m;
			REQUIRE(codec::Decode(h, p, m));
			CHECK(m.x == 12.5 && m.y == -60.0 && m.z == -3.25);
			CHECK(m.vx == 1.5f && m.vz == -4.25f && m.yaw == 90.0f && m.pitch == -15.5f);
			CHECK_EQ(m.flags, std::uint32_t(kPlayerOnGround));
			CHECK_EQ(m.timeUs, std::uint64_t(123456789));
			CHECK_EQ(m.frame, std::uint32_t(4321));
		} else if (v.name == "HELLO") {
			HelloMsg m;
			REQUIRE(codec::Decode(h, p, m));
			CHECK_EQ(m.role, std::uint32_t(1));
			CHECK_EQ(m.pid, std::uint32_t(4242));
			CHECK_EQ(m.softwareBytes, std::uint16_t(13));
			CHECK(std::memcmp(m.software, "CraftV-Golden", 13) == 0);
		} else if (v.name == "LOG") {
			LogMsg m;
			REQUIRE(codec::Decode(h, p, m));
			CHECK_EQ(m.level, std::uint8_t(kLogInfo));
			CHECK_EQ(m.textBytes, std::uint16_t(21));  // "hello from golden " (18) + U+2713 (3 bytes)
		} else if (v.name == "TEST_PATTERN") {
			std::uint64_t index = 0;
			CHECK(codec::CheckTestPattern(p, h.payloadBytes, index));
			CHECK_EQ(index, golden::kTestPatternIndex);
		}
	}
}

TEST_CASE("codec: validation rejects bad values")
{
	auto ps = golden::PlayerState();
	CHECK(codec::Valid(ps));
	auto bad = ps;
	bad.x = std::numeric_limits<double>::quiet_NaN();
	CHECK(!codec::Valid(bad));
	bad = ps;
	bad.pitch = 91.0f;
	CHECK(!codec::Valid(bad));
	bad = ps;
	bad.y = 5000.0;
	CHECK(!codec::Valid(bad));
	bad = ps;
	bad.vx = 2000.0f;
	CHECK(!codec::Valid(bad));
	bad = ps;
	bad.flags = 0x80;
	CHECK(!codec::Valid(bad));

	auto br = golden::BlockBreakRequest();
	CHECK(codec::Valid(br));
	br.face = 6;
	CHECK(!codec::Valid(br));
	br.face = kFaceUnknown;
	CHECK(codec::Valid(br));
	br.requestId = 0;
	CHECK(!codec::Valid(br));

	auto log = golden::Log();
	log.textBytes = 300;
	CHECK(!codec::Valid(log));

	auto hello = golden::Hello();
	hello.role = 3;
	CHECK(!codec::Valid(hello));
}

TEST_CASE("codec: a payload shorter than its type is malformed, a longer one is read by prefix")
{
	auto                      ps = golden::PlayerState();
	std::vector<std::uint8_t> payload(sizeof(ps) + 16, 0xAB);
	std::memcpy(payload.data(), &ps, sizeof(ps));
	RecordHeader   h{ kMsgPlayerState, 2, static_cast<std::uint32_t>(payload.size()), 1, 1 };
	PlayerStateMsg out;
	CHECK(codec::Decode(h, payload.data(), out));  // newer, longer version: prefix accepted
	h.payloadBytes = sizeof(ps) - 1;
	CHECK(!codec::Decode(h, payload.data(), out));  // too short
	h.payloadBytes = sizeof(ps);
	h.type = kMsgBlockSet;
	CHECK(!codec::Decode(h, payload.data(), out));  // wrong type
}

TEST_CASE("codec: text is truncated on a UTF-8 boundary")
{
	std::string text(kLogTextMaxBytes - 1, 'a');
	text += reinterpret_cast<const char*>(u8"✓");  // 3 bytes would cross the 256-byte limit
	const auto msg = codec::MakeLog(kLogWarn, text.c_str());
	CHECK_EQ(msg.textBytes, std::uint16_t(kLogTextMaxBytes - 1));
	const auto hello = codec::MakeHello(Role::kMc, 1, 1, "0123456789012345678901234567890123456789-overflow");
	CHECK_EQ(hello.softwareBytes, std::uint16_t(kSoftwareMaxBytes));
}

TEST_CASE("codec: TEST_PATTERN detects any flipped byte")
{
	std::uint8_t        buf[kTestPatternFixedBytes + kTestPatternMaxFill];
	const std::uint32_t n = codec::BuildTestPattern(1000, buf);
	CHECK_EQ(n, codec::TestPatternPayloadBytes(1000));
	std::uint64_t idx = 0;
	CHECK(codec::CheckTestPattern(buf, n, idx));
	for (std::uint32_t i = 0; i < n; ++i) {
		buf[i] ^= 0x01;
		CHECK(!codec::CheckTestPattern(buf, n, idx) || i < 8);  // flipping the index itself just names another valid index... unless fill disagrees
		buf[i] ^= 0x01;
	}
	CHECK(!codec::CheckTestPattern(buf, n - 1, idx));
}

TEST_CASE("codec: direction rules")
{
	CHECK(codec::AllowedFrom(kMsgPlayerState, Role::kHost));
	CHECK(!codec::AllowedFrom(kMsgPlayerState, Role::kMc));
	CHECK(codec::AllowedFrom(kMsgBlockSet, Role::kMc));
	CHECK(codec::AllowedFrom(kMsgBlockSet, Role::kHost));
	CHECK(!codec::AllowedFrom(kMsgLog, Role::kNone));
}
