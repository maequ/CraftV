// The documented field values behind protocol/golden/golden_vectors.txt (PROTOCOL.md §10).
// The Java test (GoldenVectorsTest) uses the same values; keep them identical.
#pragma once

#include "craftv/protocol.h"

#include <cstdint>
#include <string>
#include <vector>

namespace craftv::golden
{
	inline constexpr std::uint32_t kSession = 0x11223344;
	inline constexpr std::uint32_t kSeq = 7;
	inline constexpr std::uint64_t kTestPatternIndex = 5;

	struct Vector
	{
		std::string               name;
		std::vector<std::uint8_t> record;  // complete record: header + payload + padding
	};

	proto::HelloMsg             Hello();
	proto::HeartbeatMsg         Heartbeat();
	proto::PlayerStateMsg       PlayerState();
	proto::BlockSetMsg          BlockSet();
	proto::BlockBreakRequestMsg BlockBreakRequest();
	proto::BlockPlaceRequestMsg BlockPlaceRequest();
	proto::LogMsg               Log();
	proto::RemotePlayerJoinMsg  RemotePlayerJoin();
	proto::RemotePlayerStateMsg RemotePlayerState();
	proto::RemotePlayerLeaveMsg RemotePlayerLeave();
	proto::TerrainRequestMsg    TerrainRequest();
	proto::TerrainPatchMsg      TerrainPatch();
	proto::SessionInfoMsg       SessionInfo();
	proto::CameraMsg            Camera();
	proto::ViewMsg              View();
	proto::InputMsg             Input();
	proto::OwnerStateMsg        OwnerState();
	proto::BlockRegionRequestMsg BlockRegionRequest();
	proto::WorldEventMsg WorldEvent();

	// Every golden record, in file order, encoded with the C++ codec.
	std::vector<Vector> AllVectors();
}
