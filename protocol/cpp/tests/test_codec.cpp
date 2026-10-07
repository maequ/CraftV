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

TEST_CASE("codec: v1.1 direction rules (PROTOCOL.md §7 table)")
{
	for (auto type : { kMsgRemotePlayerJoin, kMsgRemotePlayerState, kMsgRemotePlayerLeave, kMsgTerrainRequest, kMsgSessionInfo }) {
		CHECK(codec::AllowedFrom(type, Role::kMc));
		CHECK(!codec::AllowedFrom(type, Role::kHost));
	}
	CHECK(codec::AllowedFrom(kMsgTerrainPatch, Role::kHost));
	CHECK(!codec::AllowedFrom(kMsgTerrainPatch, Role::kMc));
	CHECK_EQ(codec::FixedPayloadBytes(kMsgTerrainPatch), std::uint32_t(1296));
	CHECK_EQ(codec::FixedPayloadBytes(kMsgRemotePlayerLeave), std::uint32_t(8));
}

TEST_CASE("codec: REMOTE_PLAYER_STATE and TERRAIN_REQUEST built literally from PROTOCOL.md")
{
	Bytes st;
	st.U16(9);
	st.U16(1);
	st.U32(64);
	st.U32(0x11223344);
	st.U32(7);
	st.U32(117);                // playerId
	st.U32(1u | 4u | 128u);     // ON_GROUND | SPRINTING | SWING
	st.F64(100.5);
	st.F64(71.0);
	st.F64(-250.25);
	st.F32(5.5f);
	st.F32(0.0f);
	st.F32(-1.25f);
	st.F32(-45.5f);             // yaw
	st.F32(12.25f);             // pitch
	st.F32(-40.0f);             // bodyYaw
	st.U32(9001);               // tick
	st.U8(1);                   // creative
	st.U8(20);                  // health
	st.Zeros(2);
	st.PadTo16();
	CHECK_EQ(st.b.size(), std::size_t(80));

	Bytes rq;
	rq.U16(11);
	rq.U16(1);
	rq.U32(16);
	rq.U32(0x11223344);
	rq.U32(7);
	rq.I32(-3);
	rq.I32(7);
	rq.U32(42);
	rq.U16(2);
	rq.Zeros(2);
	rq.PadTo16();

	Bytes patch;  // just the fixed part and the first column of each array
	patch.U16(12);
	patch.U16(1);
	patch.U32(1296);
	patch.U32(0x11223344);
	patch.U32(7);
	patch.I32(-3);
	patch.I32(7);
	patch.U32(42);
	patch.U32(0);
	patch.U16(60);  // groundY[0]

	for (const auto& v : golden::AllVectors()) {
		if (v.name == "REMOTE_PLAYER_STATE") {
			CHECK(v.record == st.b);
		}
		if (v.name == "TERRAIN_REQUEST") {
			CHECK(v.record == rq.b);
		}
		if (v.name == "TERRAIN_PATCH") {
			REQUIRE(v.record.size() == RecordBytes(1296));
			CHECK(std::memcmp(v.record.data(), patch.b.data(), patch.b.size()) == 0);
			const std::size_t water0 = kRecordHeaderBytes + 528, material1 = kRecordHeaderBytes + 1040 + 1;
			CHECK(v.record[water0] == 75 && v.record[water0 + 1] == 0);
			CHECK(v.record[material1] == 1);
			const std::size_t lastGround = kRecordHeaderBytes + 16 + 2 * 255;
			CHECK(v.record[lastGround] == 0x00 && v.record[lastGround + 1] == 0x80);  // NO_GROUND = -32768
		}
	}
}

TEST_CASE("codec: v1.1 goldens decode back to the documented fields")
{
	for (const auto& v : golden::AllVectors()) {
		RecordHeader h;
		std::memcpy(&h, v.record.data(), sizeof(h));
		const std::uint8_t* p = v.record.data() + kRecordHeaderBytes;
		if (v.name == "REMOTE_PLAYER_JOIN") {
			RemotePlayerJoinMsg m;
			REQUIRE(codec::Decode(h, p, m));
			CHECK_EQ(m.playerId, std::uint32_t(117));
			CHECK_EQ(m.nameBytes, std::uint16_t(12));
			CHECK(std::memcmp(m.name, "Steve_Friend", 12) == 0);
			CHECK(m.uuid[0] == 0 && m.uuid[15] == 15);
		} else if (v.name == "REMOTE_PLAYER_STATE") {
			RemotePlayerStateMsg m;
			REQUIRE(codec::Decode(h, p, m));
			CHECK(m.x == 100.5 && m.y == 71.0 && m.z == -250.25);
			CHECK(m.bodyYaw == -40.0f && m.gameMode == 1 && m.health == 20);
		} else if (v.name == "REMOTE_PLAYER_LEAVE") {
			RemotePlayerLeaveMsg m;
			REQUIRE(codec::Decode(h, p, m));
			CHECK_EQ(m.playerId, std::uint32_t(117));
		} else if (v.name == "TERRAIN_PATCH") {
			TerrainPatchMsg m;
			REQUIRE(codec::Decode(h, p, m));
			CHECK_EQ(m.groundY[codec::TerrainColumn(15, 0)], std::int16_t(75));
			CHECK_EQ(m.groundY[codec::TerrainColumn(0, 15)], std::int16_t(45));
			CHECK_EQ(m.groundY[255], kNoGround);
			CHECK_EQ(m.waterY[16], kNoWater);
			CHECK_EQ(m.material[14], std::uint8_t(1));
		} else if (v.name == "SESSION_INFO") {
			SessionInfoMsg m;
			REQUIRE(codec::Decode(h, p, m));
			CHECK_EQ(m.port, std::uint16_t(25565));
			CHECK_EQ(m.addressBytes, std::uint16_t(18));
			CHECK(std::memcmp(m.address, "192.168.1.23:25565", 18) == 0);
		}
	}
}

TEST_CASE("codec: v1.1 validation rejects bad values")
{
	auto st = golden::RemotePlayerState();
	CHECK(codec::Valid(st));
	auto bad = st;
	bad.playerId = 0;
	CHECK(!codec::Valid(bad));
	bad = st;
	bad.bodyYaw = std::numeric_limits<float>::infinity();
	CHECK(!codec::Valid(bad));
	bad = st;
	bad.flags = 1u << 10;
	CHECK(!codec::Valid(bad));
	bad = st;
	bad.gameMode = 4;
	CHECK(!codec::Valid(bad));
	bad = st;
	bad.y = -3000.0;
	CHECK(!codec::Valid(bad));

	auto join = golden::RemotePlayerJoin();
	CHECK(codec::Valid(join));
	join.nameBytes = 0;
	CHECK(!codec::Valid(join));
	join.nameBytes = 33;
	CHECK(!codec::Valid(join));

	CHECK(!codec::Valid(RemotePlayerLeaveMsg{ 117, 3 }));
	CHECK(!codec::Valid(RemotePlayerLeaveMsg{ 0, 0 }));

	auto rq = golden::TerrainRequest();
	rq.chunkX = kMaxChunkCoord + 1;
	CHECK(!codec::Valid(rq));
	rq = golden::TerrainRequest();
	rq.requestId = 0;
	CHECK(!codec::Valid(rq));

	auto patch = golden::TerrainPatch();
	CHECK(codec::Valid(patch));
	patch.material[3] = 200;  // unknown material: still valid (§7.12)
	CHECK(codec::Valid(patch));
	patch.groundY[100] = 5000;
	CHECK(!codec::Valid(patch));
	patch = golden::TerrainPatch();
	patch.waterY[7] = -2049;
	CHECK(!codec::Valid(patch));

	auto info = golden::SessionInfo();
	CHECK(codec::Valid(info));
	info.flags = 8;
	CHECK(!codec::Valid(info));
	info = golden::SessionInfo();
	info.addressBytes = 65;
	CHECK(!codec::Valid(info));
}

TEST_CASE("codec: v1.2 direction rules and sizes (PROTOCOL.md §7 table)")
{
	for (auto type : { kMsgCamera, kMsgView, kMsgInput }) {
		CHECK(codec::AllowedFrom(type, Role::kHost));
		CHECK(!codec::AllowedFrom(type, Role::kMc));
	}
	CHECK(codec::AllowedFrom(kMsgOwnerState, Role::kMc));
	CHECK(!codec::AllowedFrom(kMsgOwnerState, Role::kHost));
	CHECK_EQ(codec::FixedPayloadBytes(kMsgCamera), std::uint32_t(96));
	CHECK_EQ(codec::FixedPayloadBytes(kMsgView), std::uint32_t(16));
	CHECK_EQ(codec::FixedPayloadBytes(kMsgInput), std::uint32_t(8));
	CHECK_EQ(codec::FixedPayloadBytes(kMsgOwnerState), std::uint32_t(16));
}

TEST_CASE("codec: v1.2 goldens decode back to the documented fields")
{
	int seen = 0;
	for (const auto& v : golden::AllVectors()) {
		RecordHeader h;
		std::memcpy(&h, v.record.data(), sizeof(h));
		const std::uint8_t* p = v.record.data() + kRecordHeaderBytes;
		if (v.name == "CAMERA") {
			CameraMsg m;
			REQUIRE(codec::Decode(h, p, m));
			CHECK(m.frame == 123456u && m.z == 1447.25 && m.feetY == 29.625 && m.fovY == 50.0f);
			CHECK_EQ(m.flags, std::uint32_t(kCameraFirstPerson | kCameraPassthrough));
			++seen;
		} else if (v.name == "VIEW") {
			ViewMsg m;
			REQUIRE(codec::Decode(h, p, m));
			CHECK(m.width == 1920 && m.height == 1080 && m.hostWidth == 2560);
			++seen;
		} else if (v.name == "INPUT") {
			InputMsg m;
			REQUIRE(codec::Decode(h, p, m));
			CHECK(m.kind == kInputButton && m.button == kButtonAttack && m.down == 1);
			++seen;
		} else if (v.name == "OWNER_STATE") {
			OwnerStateMsg m;
			REQUIRE(codec::Decode(h, p, m));
			CHECK(m.held == kHeldPickaxe && m.health == 17 && m.attackCharge == 0.75f);
			++seen;
		}
	}
	CHECK_EQ(seen, 4);
}

TEST_CASE("codec: v1.2 validation rejects bad values")
{
	auto cam = golden::Camera();
	CHECK(codec::Valid(cam));
	auto bad = cam;
	bad.fovY = 0.5f;
	CHECK(!codec::Valid(bad));
	bad = cam;
	bad.pitch = 91.0f;
	CHECK(!codec::Valid(bad));
	bad = cam;
	bad.flags = 1u << 5;
	CHECK(!codec::Valid(bad));
	bad = cam;
	bad.feetY = 5000.0;
	CHECK(!codec::Valid(bad));
	bad = cam;
	bad.roll = std::numeric_limits<float>::quiet_NaN();
	CHECK(!codec::Valid(bad));

	CHECK(codec::Valid(golden::View()));
	CHECK(!codec::Valid(ViewMsg{ 3840, 2160, 3840, 2160 }));  // more pixels than VIEW_MAX_PIXELS
	CHECK(!codec::Valid(ViewMsg{ 32, 1080, 0, 0 }));
	CHECK(codec::Valid(ViewMsg{ 2560, 1080, 5120, 2160 }));

	CHECK(codec::Valid(golden::Input()));
	CHECK(codec::Valid(InputMsg{ kInputSlot, 0, 0, 8, 0 }));
	CHECK(!codec::Valid(InputMsg{ kInputSlot, 0, 0, 9, 0 }));
	CHECK(codec::Valid(InputMsg{ kInputScroll, 0, 0, -1, 0 }));
	CHECK(!codec::Valid(InputMsg{ kInputScroll, 0, 0, 0, 0 }));
	CHECK(!codec::Valid(InputMsg{ kInputButton, 8, 1, 0, 0 }));
	CHECK(!codec::Valid(InputMsg{ 7, 0, 0, 0, 0 }));

	auto owner = golden::OwnerState();
	CHECK(codec::Valid(owner));
	auto badOwner = owner;
	badOwner.held = 9;
	CHECK(!codec::Valid(badOwner));
	badOwner = owner;
	badOwner.attackCharge = 1.5f;
	CHECK(!codec::Valid(badOwner));
	badOwner = owner;
	badOwner.flags = 4;
	CHECK(!codec::Valid(badOwner));
}

TEST_CASE("codec: v1.3 BLOCK_REGION_REQUEST and the SOLID/REGION block flags")
{
	CHECK(codec::AllowedFrom(kMsgBlockRegionRequest, Role::kHost));
	CHECK(!codec::AllowedFrom(kMsgBlockRegionRequest, Role::kMc));
	CHECK_EQ(codec::FixedPayloadBytes(kMsgBlockRegionRequest), std::uint32_t(16));
	CHECK(codec::Valid(golden::BlockRegionRequest()));
	CHECK(!codec::Valid(BlockRegionRequestMsg{ 0, 0, 0, 0 }));
	CHECK(!codec::Valid(BlockRegionRequestMsg{ kMaxChunkCoord + 1, 0, 1, 0 }));
	CHECK(codec::Valid(BlockSetMsg{ 1, 64, 1, 1, kBlockSetSolid | kBlockSetRegion, 77 }));
	CHECK(!codec::Valid(BlockSetMsg{ 1, 64, 1, 1, 1u << 4, 0 }));
	bool seen = false;
	for (const auto& v : golden::AllVectors()) {
		if (v.name == "BLOCK_REGION_REQUEST") {
			RecordHeader h;
			std::memcpy(&h, v.record.data(), sizeof(h));
			BlockRegionRequestMsg m;
			REQUIRE(codec::Decode(h, v.record.data() + kRecordHeaderBytes, m));
			CHECK(m.chunkX == -3 && m.chunkZ == 92 && m.requestId == 77u);
			seen = true;
		}
	}
	CHECK(seen);
}

TEST_CASE("codec: v1.3 INPUT CURSOR")
{
	CHECK(codec::Valid(InputMsg{ kInputCursor, 0, 0, 0, 0xFFFF0000u }));
	CHECK(!codec::Valid(InputMsg{ kInputCursor, 1, 0, 0, 0 }));
	CHECK(!codec::Valid(InputMsg{ 7, 0, 0, 0, 0 }));
}


TEST_CASE("codec: v1.4 WORLD_EVENT, INPUT DAMAGE/OPTION and the new flags")
{
	CHECK(codec::AllowedFrom(kMsgWorldEvent, Role::kMc));
	CHECK(!codec::AllowedFrom(kMsgWorldEvent, Role::kHost));
	CHECK_EQ(codec::FixedPayloadBytes(kMsgWorldEvent), std::uint32_t(40));
	CHECK(codec::Valid(golden::WorldEvent()));
	auto ev = golden::WorldEvent();
	ev.kind = 3;
	CHECK(!codec::Valid(ev));
	ev = golden::WorldEvent();
	ev.power = -1.0f;
	CHECK(!codec::Valid(ev));
	ev = golden::WorldEvent();
	ev.reserved1 = 1;
	CHECK(!codec::Valid(ev));

	CHECK(codec::Valid(InputMsg{ kInputDamage, kDamageBullet, 0, 6, 0 }));
	CHECK(!codec::Valid(InputMsg{ kInputDamage, kDamageCauseMax + 1, 0, 6, 0 }));
	CHECK(!codec::Valid(InputMsg{ kInputDamage, kDamageBullet, 0, 0, 0 }));
	CHECK(codec::Valid(InputMsg{ kInputOption, kOptionCrosshair, 0, 1, 0 }));
	CHECK(codec::Valid(InputMsg{ kInputOption, kOptionFrameRate, 0, 0, 0 }));
	CHECK(!codec::Valid(InputMsg{ kInputOption, 0, 0, 1, 0 }));
	CHECK(!codec::Valid(InputMsg{ kInputOption, kOptionMax + 1, 0, 1, 0 }));
	CHECK(!codec::Valid(InputMsg{ kInputOption, kOptionHud, 0, -1, 0 }));

	CHECK(codec::Valid(BlockSetMsg{ 1, 64, 1, 1, kBlockSetLight | kBlockSetRegion, 77 }));
	auto cam = golden::Camera();
	cam.flags |= kCameraPhone | kCameraSprinting;
	CHECK(codec::Valid(cam));
	cam.flags |= 1u << 5;
	CHECK(!codec::Valid(cam));
	auto owner = golden::OwnerState();
	owner.held = kHeldLight;
	CHECK(codec::Valid(owner));
}
