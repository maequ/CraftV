#include "golden_values.h"

#include "redcraft/codec.h"

namespace redcraft::golden
{
	using namespace redcraft::proto;

	HelloMsg Hello()
	{
		return codec::MakeHello(Role::kHost, 4242, kSession, "RedCraft-Golden");
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

		std::uint8_t        payload[kTestPatternFixedBytes + kTestPatternMaxFill];
		const std::uint32_t bytes = codec::BuildTestPattern(kTestPatternIndex, payload);
		Vector              tp{ "TEST_PATTERN", std::vector<std::uint8_t>(RecordBytes(bytes)) };
		codec::EncodeRecord(tp.record.data(), tp.record.size(), kMsgTestPattern, kTypeVersion1, kSession, kSeq, payload, bytes);
		all.push_back(tp);
		return all;
	}
}
