#include "golden_values.h"

#include "craftv/codec.h"

namespace craftv::golden
{
	using namespace craftv::proto;

	HelloMsg Hello()
	{
		return codec::MakeHello(Role::kHost, 4242, kSession, "CraftV-Golden");
	}

	HeartbeatMsg Heartbeat()
	{
		HeartbeatMsg m{};
		m.sentUs = 1000000;
		m.echoUs = 999000;
		m.echoHoldUs = 250;
		m.counter = 77;
		return m;
	}

	PlayerStateMsg PlayerState()
	{
		PlayerStateMsg m{};
		m.x = 12.5;
		m.y = -60.0;
		m.z = -3.25;
		m.vx = 1.5f;
		m.vy = 0.0f;
		m.vz = -4.25f;
		m.yaw = 90.0f;
		m.pitch = -15.5f;
		m.flags = kPlayerOnGround;
		m.timeUs = 123456789;
		m.frame = 4321;
		return m;
	}

	BlockSetMsg BlockSet()
	{
		return BlockSetMsg{ -5, 64, 1024, 1, kBlockSetEcho, 9 };
	}

	BlockBreakRequestMsg BlockBreakRequest()
	{
		BlockBreakRequestMsg m{};
		m.requestId = 3;
		m.x = 10;
		m.y = -61;
		m.z = -20;
		m.face = 1;
		m.flags = 0;
		return m;
	}

	BlockPlaceRequestMsg BlockPlaceRequest()
	{
		BlockPlaceRequestMsg m{};
		m.requestId = 4;
		m.x = 10;
		m.y = -61;
		m.z = -20;
		m.face = 1;
		m.blockId = 1;
		return m;
	}

	LogMsg Log()
	{
		// u8 literal: includes a non-ASCII character (U+2713) to pin UTF-8 handling on both sides.
		return codec::MakeLog(kLogInfo, reinterpret_cast<const char*>(u8"hello from golden ✓"));
	}

	// v1.1 (PROTOCOL.md §7.8-7.13)
	RemotePlayerJoinMsg RemotePlayerJoin()
	{
		RemotePlayerJoinMsg m{};
		m.playerId = 117;
		for (std::uint8_t i = 0; i < kUuidBytes; ++i) {
			m.uuid[i] = i;  // 00010203-0405-0607-0809-0a0b0c0d0e0f
		}
		codec::SetPlayerName(m, "Steve_Friend");
		return m;
	}

	RemotePlayerStateMsg RemotePlayerState()
	{
		RemotePlayerStateMsg m{};
		m.playerId = 117;
		m.flags = kRemoteOnGround | kRemoteSprinting | kRemoteSwing;
		m.x = 100.5;
		m.y = 71.0;
		m.z = -250.25;
		m.vx = 5.5f;
		m.vy = 0.0f;
		m.vz = -1.25f;
		m.yaw = -45.5f;
		m.pitch = 12.25f;
		m.bodyYaw = -40.0f;
		m.tick = 9001;
		m.gameMode = 1;
		m.health = 20;
		return m;
	}

	RemotePlayerLeaveMsg RemotePlayerLeave()
	{
		return RemotePlayerLeaveMsg{ 117, kLeaveLeft };
	}

	TerrainRequestMsg TerrainRequest()
	{
		TerrainRequestMsg m{};
		m.chunkX = -3;
		m.chunkZ = 7;
		m.requestId = 42;
		m.distance = 2;
		return m;
	}

	TerrainPatchMsg TerrainPatch()
	{
		TerrainPatchMsg m{};
		m.chunkX = -3;
		m.chunkZ = 7;
		m.requestId = 42;
		for (std::uint32_t i = 0; i < kChunkColumns; ++i) {
			m.groundY[i] = static_cast<std::int16_t>(60 + static_cast<int>(i & 15) - static_cast<int>(i >> 4));
			m.waterY[i] = i < 16 ? std::int16_t(75) : kNoWater;
			m.material[i] = static_cast<std::uint8_t>(i % 13);
		}
		m.groundY[255] = kNoGround;
		return m;
	}

	SessionInfoMsg SessionInfo()
	{
		SessionInfoMsg m{};
		m.flags = kSessionOpen | kSessionAuth;
		m.port = 25565;
		m.friends = 2;
		m.maxPlayers = 8;
		m.gameMode = 1;
		codec::SetAddress(m, "192.168.1.23:25565");
		return m;
	}

	CameraMsg Camera()
	{
		CameraMsg m{};
		m.frame = 123456;
		m.timeUs = 987654321;
		m.x = -16.5;
		m.y = 31.625;
		m.z = 1447.25;
		m.yaw = 135.5f;
		m.pitch = -12.25f;
		m.roll = 1.5f;
		m.fovY = 50.0f;
		m.feetX = -16.0;
		m.feetY = 29.625;
		m.feetZ = 1446.0;
		m.bodyYaw = 130.0f;
		m.flags = kCameraFirstPerson | kCameraPassthrough;
		m.nearClip = 0.25f;
		m.farClip = 10000.0f;
		return m;
	}

	ViewMsg View()
	{
		return ViewMsg{ 1920, 1080, 2560, 1440 };
	}

	InputMsg Input()
	{
		InputMsg m{};
		m.kind = kInputButton;
		m.button = kButtonAttack;
		m.down = 1;
		return m;
	}

	OwnerStateMsg OwnerState()
	{
		OwnerStateMsg m{};
		m.held = kHeldPickaxe;
		m.health = 17;
		m.food = 18;
		m.gameMode = 0;
		m.attackDamage = 5.0f;
		m.attackCharge = 0.75f;
		return m;
	}

	BlockRegionRequestMsg BlockRegionRequest()
	{
		return BlockRegionRequestMsg{ -3, 92, 77, 0 };
	}

	namespace
	{
		template <class T>
		Vector Make(const char* a_name, const T& a_msg)
		{
			Vector v{ a_name, std::vector<std::uint8_t>(RecordBytes(sizeof(T))) };
			codec::EncodeRecord(v.record.data(), v.record.size(), T::kType, kTypeVersion1, kSession, kSeq, &a_msg, sizeof(T));
			return v;
		}
	}

	std::vector<Vector> AllVectors()
	{
		std::vector<Vector> all;
		all.push_back(Make("HELLO", Hello()));
		all.push_back(Make("HEARTBEAT", Heartbeat()));
		all.push_back(Make("PLAYER_STATE", PlayerState()));
		all.push_back(Make("BLOCK_SET", BlockSet()));
		all.push_back(Make("BLOCK_BREAK_REQUEST", BlockBreakRequest()));
		all.push_back(Make("BLOCK_PLACE_REQUEST", BlockPlaceRequest()));
		all.push_back(Make("LOG", Log()));
		all.push_back(Make("REMOTE_PLAYER_JOIN", RemotePlayerJoin()));
		all.push_back(Make("REMOTE_PLAYER_STATE", RemotePlayerState()));
		all.push_back(Make("REMOTE_PLAYER_LEAVE", RemotePlayerLeave()));
		all.push_back(Make("TERRAIN_REQUEST", TerrainRequest()));
		all.push_back(Make("TERRAIN_PATCH", TerrainPatch()));
		all.push_back(Make("SESSION_INFO", SessionInfo()));
		all.push_back(Make("CAMERA", Camera()));
		all.push_back(Make("VIEW", View()));
		all.push_back(Make("INPUT", Input()));
		all.push_back(Make("OWNER_STATE", OwnerState()));
		all.push_back(Make("BLOCK_REGION_REQUEST", BlockRegionRequest()));

		std::uint8_t        payload[kTestPatternFixedBytes + kTestPatternMaxFill];
		const std::uint32_t bytes = codec::BuildTestPattern(kTestPatternIndex, payload);
		Vector              tp{ "TEST_PATTERN", std::vector<std::uint8_t>(RecordBytes(bytes)) };
		codec::EncodeRecord(tp.record.data(), tp.record.size(), kMsgTestPattern, kTypeVersion1, kSession, kSeq, payload, bytes);
		all.push_back(tp);
		return all;
	}
}
