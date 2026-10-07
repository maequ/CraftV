#include "craftv/codec.h"

#include <cmath>

namespace craftv::codec
{
	namespace
	{
		bool Finite(float a_value) { return std::isfinite(a_value); }
		bool Finite(double a_value) { return std::isfinite(a_value); }

		bool FaceOk(std::uint8_t a_face) { return a_face <= kFaceMax || a_face == kFaceUnknown; }

		void CopyText(char* a_dst, std::size_t a_cap, const char* a_src, std::uint16_t& a_bytes)
		{
			std::size_t n = a_src ? std::strlen(a_src) : 0;
			if (n > a_cap) {
				n = a_cap;
				// Don't cut a UTF-8 sequence in half: back up to a lead byte.
				while (n > 0 && (static_cast<unsigned char>(a_src[n]) & 0xC0) == 0x80) {
					--n;
				}
			}
			std::memset(a_dst, 0, a_cap);
			if (n) {
				std::memcpy(a_dst, a_src, n);
			}
			a_bytes = static_cast<std::uint16_t>(n);
		}
	}

	std::size_t EncodeRecord(std::uint8_t* a_out, std::size_t a_cap, std::uint16_t a_type, std::uint16_t a_typeVersion,
		std::uint32_t a_session, std::uint32_t a_seq, const void* a_payload, std::uint32_t a_payloadBytes)
	{
		if (a_payloadBytes > kMaxPayload) {
			return 0;
		}
		const auto size = static_cast<std::size_t>(RecordBytes(a_payloadBytes));
		if (size > a_cap) {
			return 0;
		}
		const RecordHeader header{ a_type, a_typeVersion, a_payloadBytes, a_session, a_seq };
		std::memcpy(a_out, &header, sizeof(header));
		if (a_payloadBytes) {
			std::memcpy(a_out + kRecordHeaderBytes, a_payload, a_payloadBytes);
		}
		std::memset(a_out + kRecordHeaderBytes + a_payloadBytes, 0, size - kRecordHeaderBytes - a_payloadBytes);
		return size;
	}

	std::uint32_t FixedPayloadBytes(std::uint16_t a_type)
	{
		switch (a_type) {
		case kMsgHello:
			return sizeof(HelloMsg);
		case kMsgHeartbeat:
			return sizeof(HeartbeatMsg);
		case kMsgPlayerState:
			return sizeof(PlayerStateMsg);
		case kMsgBlockSet:
			return sizeof(BlockSetMsg);
		case kMsgBlockBreakRequest:
			return sizeof(BlockBreakRequestMsg);
		case kMsgBlockPlaceRequest:
			return sizeof(BlockPlaceRequestMsg);
		case kMsgLog:
			return sizeof(LogMsg);
		case kMsgRemotePlayerJoin:
			return sizeof(RemotePlayerJoinMsg);
		case kMsgRemotePlayerState:
			return sizeof(RemotePlayerStateMsg);
		case kMsgRemotePlayerLeave:
			return sizeof(RemotePlayerLeaveMsg);
		case kMsgTerrainRequest:
			return sizeof(TerrainRequestMsg);
		case kMsgTerrainPatch:
			return sizeof(TerrainPatchMsg);
		case kMsgSessionInfo:
			return sizeof(SessionInfoMsg);
		case kMsgCamera:
			return sizeof(CameraMsg);
		case kMsgView:
			return sizeof(ViewMsg);
		case kMsgInput:
			return sizeof(InputMsg);
		case kMsgOwnerState:
			return sizeof(OwnerStateMsg);
		case kMsgBlockRegionRequest:
			return sizeof(BlockRegionRequestMsg);
		case kMsgWorldEvent:
			return sizeof(WorldEventMsg);
		default:
			return 0;
		}
	}

	bool AllowedFrom(std::uint16_t a_type, Role a_sender)
	{
		if (a_sender != Role::kHost && a_sender != Role::kMc) {
			return false;
		}
		switch (a_type) {
		case kMsgHello:
		case kMsgHeartbeat:
		case kMsgBlockSet:
		case kMsgBlockBreakRequest:
		case kMsgBlockPlaceRequest:
		case kMsgLog:
		case kMsgTestPattern:
			return true;
		case kMsgRemotePlayerJoin:
		case kMsgRemotePlayerState:
		case kMsgRemotePlayerLeave:
		case kMsgTerrainRequest:
		case kMsgSessionInfo:
		case kMsgOwnerState:
		case kMsgWorldEvent:
			return a_sender == Role::kMc;  // §7.8-7.11, §7.13, §7.18, §7.20
		case kMsgTerrainPatch:
		case kMsgCamera:
		case kMsgView:
		case kMsgInput:
		case kMsgBlockRegionRequest:
			return a_sender == Role::kHost;  // §7.12, §7.15-7.17, §7.19
		case kMsgPlayerState:
			// MC -> host is reserved for Phase 3 (PROTOCOL.md §7.3); a v1.0 receiver only takes the host's.
			return a_sender == Role::kHost;
		default:
			return true;  // unknown types are skipped by length, not rejected here
		}
	}

	bool Valid(const HelloMsg& a_msg)
	{
		return (a_msg.role == static_cast<std::uint32_t>(Role::kHost) || a_msg.role == static_cast<std::uint32_t>(Role::kMc)) &&
		       a_msg.softwareBytes <= kSoftwareMaxBytes && a_msg.session != 0;
	}

	bool Valid(const HeartbeatMsg& a_msg)
	{
		return a_msg.sentUs != 0;
	}

	bool Valid(const PlayerStateMsg& a_msg)
	{
		if (!Finite(a_msg.x) || !Finite(a_msg.y) || !Finite(a_msg.z) || !Finite(a_msg.vx) || !Finite(a_msg.vy) ||
			!Finite(a_msg.vz) || !Finite(a_msg.yaw) || !Finite(a_msg.pitch)) {
			return false;
		}
		if (std::fabs(a_msg.x) > kMaxHorizontalCoord || std::fabs(a_msg.z) > kMaxHorizontalCoord || a_msg.y < kMinY || a_msg.y > kMaxY) {
			return false;
		}
		if (a_msg.pitch < -90.0f || a_msg.pitch > 90.0f) {
			return false;
		}
		const float speed2 = a_msg.vx * a_msg.vx + a_msg.vy * a_msg.vy + a_msg.vz * a_msg.vz;
		return speed2 <= kMaxSpeed * kMaxSpeed && (a_msg.flags & ~kPlayerKnownFlags) == 0;
	}

	bool Valid(const BlockSetMsg& a_msg)
	{
		return a_msg.y >= static_cast<std::int32_t>(kMinY) && a_msg.y <= static_cast<std::int32_t>(kMaxY) &&
		       a_msg.x >= -static_cast<std::int32_t>(kMaxHorizontalCoord) && a_msg.x <= static_cast<std::int32_t>(kMaxHorizontalCoord) &&
		       a_msg.z >= -static_cast<std::int32_t>(kMaxHorizontalCoord) && a_msg.z <= static_cast<std::int32_t>(kMaxHorizontalCoord) &&
		       (a_msg.flags & ~kBlockSetKnownFlags) == 0;
	}

	namespace
	{
		template <class T>
		bool ValidRequestCoords(const T& a_msg)
		{
			return a_msg.requestId != 0 && FaceOk(a_msg.face) && a_msg.y >= static_cast<std::int32_t>(kMinY) &&
			       a_msg.y <= static_cast<std::int32_t>(kMaxY) && a_msg.x >= -static_cast<std::int32_t>(kMaxHorizontalCoord) &&
			       a_msg.x <= static_cast<std::int32_t>(kMaxHorizontalCoord) && a_msg.z >= -static_cast<std::int32_t>(kMaxHorizontalCoord) &&
			       a_msg.z <= static_cast<std::int32_t>(kMaxHorizontalCoord);
		}
	}

	bool Valid(const BlockBreakRequestMsg& a_msg) { return ValidRequestCoords(a_msg); }
	bool Valid(const BlockPlaceRequestMsg& a_msg) { return ValidRequestCoords(a_msg); }

	bool Valid(const LogMsg& a_msg)
	{
		return a_msg.level <= kLogError && a_msg.textBytes <= kLogTextMaxBytes;
	}

	namespace
	{
		bool PositionOk(double a_x, double a_y, double a_z)
		{
			return Finite(a_x) && Finite(a_y) && Finite(a_z) && std::fabs(a_x) <= kMaxHorizontalCoord && std::fabs(a_z) <= kMaxHorizontalCoord &&
			       a_y >= kMinY && a_y <= kMaxY;
		}

		bool ChunkOk(std::int32_t a_chunkX, std::int32_t a_chunkZ)
		{
			return a_chunkX >= -kMaxChunkCoord && a_chunkX <= kMaxChunkCoord && a_chunkZ >= -kMaxChunkCoord && a_chunkZ <= kMaxChunkCoord;
		}

		bool HeightOk(std::int16_t a_y, std::int16_t a_sentinel)
		{
			return a_y == a_sentinel || (a_y >= static_cast<std::int16_t>(kMinY) && a_y <= static_cast<std::int16_t>(kMaxY));
		}
	}

	bool Valid(const RemotePlayerJoinMsg& a_msg)
	{
		return a_msg.playerId != 0 && a_msg.nameBytes >= 1 && a_msg.nameBytes <= kPlayerNameMaxBytes;
	}

	bool Valid(const RemotePlayerStateMsg& a_msg)
	{
		if (a_msg.playerId == 0 || !PositionOk(a_msg.x, a_msg.y, a_msg.z)) {
			return false;
		}
		if (!Finite(a_msg.vx) || !Finite(a_msg.vy) || !Finite(a_msg.vz) || !Finite(a_msg.yaw) || !Finite(a_msg.pitch) || !Finite(a_msg.bodyYaw)) {
			return false;
		}
		const float speed2 = a_msg.vx * a_msg.vx + a_msg.vy * a_msg.vy + a_msg.vz * a_msg.vz;
		return a_msg.pitch >= -90.0f && a_msg.pitch <= 90.0f && speed2 <= kMaxSpeed * kMaxSpeed && (a_msg.flags & ~kRemoteKnownFlags) == 0 &&
		       a_msg.gameMode <= kGameModeMax;
	}

	bool Valid(const RemotePlayerLeaveMsg& a_msg)
	{
		return a_msg.playerId != 0 && a_msg.reason <= kLeaveReset;
	}

	bool Valid(const TerrainRequestMsg& a_msg)
	{
		return a_msg.requestId != 0 && ChunkOk(a_msg.chunkX, a_msg.chunkZ);
	}

	bool Valid(const TerrainPatchMsg& a_msg)
	{
		if (!ChunkOk(a_msg.chunkX, a_msg.chunkZ)) {
			return false;
		}
		for (std::uint32_t i = 0; i < kChunkColumns; ++i) {
			if (!HeightOk(a_msg.groundY[i], kNoGround) || !HeightOk(a_msg.waterY[i], kNoWater)) {
				return false;
			}
		}
		return true;  // unknown materials read as UNKNOWN (§7.12)
	}

	bool Valid(const SessionInfoMsg& a_msg)
	{
		return (a_msg.flags & ~kSessionKnownFlags) == 0 && a_msg.gameMode <= kGameModeMax && a_msg.addressBytes <= kAddressMaxBytes;
	}

	bool Valid(const CameraMsg& a_msg)
	{
		if (!PositionOk(a_msg.x, a_msg.y, a_msg.z) || !PositionOk(a_msg.feetX, a_msg.feetY, a_msg.feetZ)) {
			return false;
		}
		if (!Finite(a_msg.yaw) || !Finite(a_msg.pitch) || !Finite(a_msg.roll) || !Finite(a_msg.fovY) || !Finite(a_msg.bodyYaw) ||
			!Finite(a_msg.nearClip) || !Finite(a_msg.farClip)) {
			return false;
		}
		return a_msg.pitch >= -90.0f && a_msg.pitch <= 90.0f && a_msg.fovY >= kMinFov && a_msg.fovY <= kMaxFov &&
		       (a_msg.flags & ~kCameraKnownFlags) == 0 && a_msg.nearClip >= 0.0f && a_msg.farClip >= 0.0f;
	}

	bool Valid(const ViewMsg& a_msg)
	{
		return a_msg.width >= kViewMinSide && a_msg.width <= kViewMaxWidth && a_msg.height >= kViewMinSide && a_msg.height <= kViewMaxHeight &&
		       std::uint64_t(a_msg.width) * a_msg.height <= kViewMaxPixels && a_msg.hostWidth <= kHostMaxSide && a_msg.hostHeight <= kHostMaxSide;
	}

	bool Valid(const InputMsg& a_msg)
	{
		switch (a_msg.kind) {
		case kInputButton:
			return a_msg.button >= kButtonAttack && a_msg.button <= kButtonCloseScreen && a_msg.down <= 1 && a_msg.value == 0;
		case kInputSlot:
			return a_msg.button == 0 && a_msg.down == 0 && a_msg.value >= 0 && a_msg.value < kHotbarSlots;
		case kInputScroll:
			return a_msg.button == 0 && a_msg.down == 0 && a_msg.value != 0 && a_msg.value >= -kHotbarSlots && a_msg.value <= kHotbarSlots;
		case kInputCursor:
			return a_msg.button == 0 && a_msg.down == 0 && a_msg.value == 0;
		case kInputDamage:
			return a_msg.button <= kDamageCauseMax && a_msg.down == 0 && a_msg.value >= 1 && a_msg.cursor == 0;
		case kInputOption:
			return a_msg.button >= 1 && a_msg.button <= kOptionMax && a_msg.down == 0 && a_msg.value >= 0 && a_msg.cursor == 0;
		default:
			return false;
		}
	}

	bool Valid(const OwnerStateMsg& a_msg)
	{
		return a_msg.held <= kHeldMax && a_msg.food <= kMaxFood && a_msg.gameMode <= kGameModeMax && Finite(a_msg.attackDamage) &&
		       a_msg.attackDamage >= 0.0f && a_msg.attackDamage <= kMaxAttackDamage && Finite(a_msg.attackCharge) && a_msg.attackCharge >= 0.0f &&
		       a_msg.attackCharge <= 1.0f && (a_msg.flags & ~kOwnerKnownFlags) == 0;
	}

	bool Valid(const BlockRegionRequestMsg& a_msg)
	{
		return a_msg.requestId != 0 && ChunkOk(a_msg.chunkX, a_msg.chunkZ);
	}

	bool Valid(const WorldEventMsg& a_msg)
	{
		return a_msg.kind >= kEventExplosion && a_msg.kind <= kEventKindMax && a_msg.reserved0[0] == 0 && a_msg.reserved0[1] == 0 &&
		       a_msg.reserved0[2] == 0 && a_msg.reserved1 == 0 && PositionOk(a_msg.x, a_msg.y, a_msg.z) && Finite(a_msg.power) && a_msg.power >= 0.0f &&
		       a_msg.power <= kMaxEventPower;
	}

	void SetPlayerName(RemotePlayerJoinMsg& a_msg, const char* a_name)
	{
		CopyText(a_msg.name, sizeof(a_msg.name), a_name, a_msg.nameBytes);
	}

	void SetAddress(SessionInfoMsg& a_msg, const char* a_address)
	{
		CopyText(a_msg.address, sizeof(a_msg.address), a_address, a_msg.addressBytes);
	}

	HelloMsg MakeHello(Role a_role, std::uint32_t a_pid, std::uint32_t a_session, const char* a_software)
	{
		HelloMsg msg{};
		msg.versionMajor = kVersionMajor;
		msg.versionMinor = kVersionMinor;
		msg.role = static_cast<std::uint32_t>(a_role);
		msg.pid = a_pid;
		msg.session = a_session;
		msg.capabilities = 0;
		CopyText(msg.software, sizeof(msg.software), a_software, msg.softwareBytes);
		return msg;
	}

	LogMsg MakeLog(LogLevel a_level, const char* a_text)
	{
		LogMsg msg{};
		msg.level = a_level;
		CopyText(msg.text, sizeof(msg.text), a_text, msg.textBytes);
		return msg;
	}

	std::uint32_t Fnv1a32(const std::uint8_t* a_data, std::size_t a_bytes)
	{
		constexpr std::uint32_t kOffsetBasis = 0x811C9DC5u;
		constexpr std::uint32_t kPrime = 0x01000193u;
		std::uint32_t           hash = kOffsetBasis;
		for (std::size_t i = 0; i < a_bytes; ++i) {
			hash ^= a_data[i];
			hash *= kPrime;
		}
		return hash;
	}

	std::uint32_t TestPatternPayloadBytes(std::uint64_t a_index)
	{
		return kTestPatternFixedBytes + static_cast<std::uint32_t>(a_index % kTestPatternFillModulus);
	}

	namespace
	{
		constexpr std::uint64_t kTestPatternIndexMul = 31;
		std::uint8_t            FillByte(std::uint64_t a_index, std::uint32_t a_i)
		{
			return static_cast<std::uint8_t>((a_index * kTestPatternIndexMul + a_i) & 0xFF);
		}
	}

	std::uint32_t BuildTestPattern(std::uint64_t a_index, std::uint8_t* a_out)
	{
		const std::uint32_t fill = TestPatternPayloadBytes(a_index) - kTestPatternFixedBytes;
		std::uint8_t*       bytes = a_out + kTestPatternFixedBytes;
		for (std::uint32_t i = 0; i < fill; ++i) {
			bytes[i] = FillByte(a_index, i);
		}
		const TestPatternFixed fixed{ a_index, fill, Fnv1a32(bytes, fill) };
		std::memcpy(a_out, &fixed, sizeof(fixed));
		return kTestPatternFixedBytes + fill;
	}

	bool CheckTestPattern(const std::uint8_t* a_payload, std::uint32_t a_payloadBytes, std::uint64_t& a_index)
	{
		if (a_payloadBytes < kTestPatternFixedBytes) {
			return false;
		}
		TestPatternFixed fixed;
		std::memcpy(&fixed, a_payload, sizeof(fixed));
		if (fixed.fillBytes > kTestPatternMaxFill || kTestPatternFixedBytes + fixed.fillBytes != a_payloadBytes) {
			return false;
		}
		const std::uint8_t* bytes = a_payload + kTestPatternFixedBytes;
		for (std::uint32_t i = 0; i < fixed.fillBytes; ++i) {
			if (bytes[i] != FillByte(fixed.index, i)) {
				return false;
			}
		}
		if (Fnv1a32(bytes, fixed.fillBytes) != fixed.checksum) {
			return false;
		}
		a_index = fixed.index;
		return true;
	}
}
