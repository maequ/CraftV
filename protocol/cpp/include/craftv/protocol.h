// CraftV shared-memory protocol v1.1: constants and byte layouts.
//
// Source of truth: docs/PROTOCOL.md. Every struct here mirrors a table in that file, and the
// static_asserts below pin each size and offset to it. The Java mirror is
// fabric/src/main/java/dev/craftv/link/Proto.java. Change all three together (PROTOCOL.md §9).
//
// Lineage: the ring algorithm and liveness rules follow SkyCraft's protocol v11 (MIT, chasmlol).
#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>

namespace craftv::proto
{
	static_assert(std::endian::native == std::endian::little, "CraftV requires a little-endian machine (PROTOCOL.md §1)");

	// ---- identity (PROTOCOL.md §2, §3) --------------------------------------------------------
	inline constexpr std::uint32_t kMagic = 0x56465243;  // bytes 43 52 46 56 = "CRFV"
	inline constexpr std::uint16_t kVersionMajor = 1;
	inline constexpr std::uint16_t kVersionMinor = 3;
	inline constexpr wchar_t       kDefaultMappingName[] = L"Local\\CraftV_Shared_v1";

	enum class Role : std::uint32_t
	{
		kNone = 0,
		kHost = 1,
		kMc = 2,
	};

	// ---- region map (PROTOCOL.md §2.1) -----------------------------------------------------------
	inline constexpr std::uint64_t kHeaderBytes = 0x1000;
	inline constexpr std::uint64_t kSectionAlign = 0x1000;
	inline constexpr std::uint64_t kRingControlBytes = 0x1000;
	inline constexpr std::uint64_t kRingDataBytes = 0x100000;  // 1 MiB, power of two
	inline constexpr std::uint64_t kRingSectionBytes = kRingControlBytes + kRingDataBytes;
	inline constexpr std::uint64_t kOffRingHostToMc = 0x1000;
	inline constexpr std::uint64_t kOffRingMcToHost = kOffRingHostToMc + kRingSectionBytes;
	inline constexpr std::uint64_t kMappingBytes = kOffRingMcToHost + kRingSectionBytes;
	inline constexpr std::uint32_t kSectionCount = 2;
	inline constexpr std::uint32_t kMaxSections = 16;
	inline constexpr std::uint64_t kMinRingDataBytes = 64ull * 1024;
	inline constexpr std::uint64_t kMaxRingDataBytes = 64ull * 1024 * 1024;
	static_assert(kOffRingMcToHost == 0x102000);
	static_assert(kMappingBytes == 0x203000);

	enum SectionId : std::uint32_t
	{
		kSectionRingHostToMc = 1,
		kSectionRingMcToHost = 2,
	};

	// ---- side block (PROTOCOL.md §3.1) -----------------------------------------------------------
	enum SideState : std::uint32_t
	{
		kSideAttached = 1u << 0,
		kSideInGame = 1u << 1,
	};

	struct SideBlock
	{
		std::uint32_t pid;
		std::uint32_t session;      // 0 = never attached
		std::uint64_t heartbeat;    // counter, release-stored
		std::uint64_t heartbeatUs;  // QPC µs at the last increment (informational)
		std::uint32_t state;        // SideState bits
		std::uint8_t  reserved[36];
	};
	static_assert(sizeof(SideBlock) == 64);
	static_assert(offsetof(SideBlock, pid) == 0x00);
	static_assert(offsetof(SideBlock, session) == 0x04);
	static_assert(offsetof(SideBlock, heartbeat) == 0x08);
	static_assert(offsetof(SideBlock, heartbeatUs) == 0x10);
	static_assert(offsetof(SideBlock, state) == 0x18);

	// ---- section table entry (PROTOCOL.md §3.2) --------------------------------------------------
	struct SectionEntry
	{
		std::uint32_t id;  // SectionId
		std::uint32_t flags;
		std::uint64_t offset;  // from the mapping base, 4 KiB aligned
		std::uint64_t bytes;   // including the ring control page
		std::uint64_t reserved;
	};
	static_assert(sizeof(SectionEntry) == 32);
	static_assert(offsetof(SectionEntry, offset) == 0x08);
	static_assert(offsetof(SectionEntry, bytes) == 0x10);

	// ---- header (PROTOCOL.md §3) -----------------------------------------------------------------
	inline constexpr std::uint64_t kOffHostBlock = 0x040;
	inline constexpr std::uint64_t kOffMcBlock = 0x080;
	inline constexpr std::uint64_t kOffSectionTable = 0x100;

	struct Header
	{
		std::uint32_t magic;  // written last by the creator (release)
		std::uint16_t versionMajor;
		std::uint16_t versionMinor;
		std::uint32_t headerBytes;
		std::uint32_t sectionCount;
		std::uint64_t mappingBytes;
		std::uint32_t creatorRole;
		std::uint32_t creatorPid;
		std::uint8_t  reserved0[32];
		SideBlock     host;
		SideBlock     mc;
		std::uint8_t  reserved1[64];
		SectionEntry  sections[kMaxSections];
		std::uint8_t  reserved2[kHeaderBytes - kOffSectionTable - sizeof(SectionEntry) * kMaxSections];
	};
	static_assert(sizeof(Header) == kHeaderBytes);
	static_assert(offsetof(Header, magic) == 0x000);
	static_assert(offsetof(Header, versionMajor) == 0x004);
	static_assert(offsetof(Header, versionMinor) == 0x006);
	static_assert(offsetof(Header, headerBytes) == 0x008);
	static_assert(offsetof(Header, sectionCount) == 0x00C);
	static_assert(offsetof(Header, mappingBytes) == 0x010);
	static_assert(offsetof(Header, creatorRole) == 0x018);
	static_assert(offsetof(Header, creatorPid) == 0x01C);
	static_assert(offsetof(Header, host) == kOffHostBlock);
	static_assert(offsetof(Header, mc) == kOffMcBlock);
	static_assert(offsetof(Header, sections) == kOffSectionTable);

	// ---- ring control page (PROTOCOL.md §4.1) ----------------------------------------------------
	struct RingControl
	{
		std::uint64_t head;  // producer-written, total bytes published
		std::uint8_t  pad0[56];
		std::uint64_t tail;  // consumer-written, total bytes consumed
		std::uint8_t  pad1[56];
		std::uint64_t dataBytes;     // creator-written
		std::uint32_t producerRole;  // creator-written
		std::uint8_t  reserved[kRingControlBytes - 0x8C];
	};
	static_assert(sizeof(RingControl) == kRingControlBytes);
	static_assert(offsetof(RingControl, head) == 0x00);
	static_assert(offsetof(RingControl, tail) == 0x40);
	static_assert(offsetof(RingControl, dataBytes) == 0x80);
	static_assert(offsetof(RingControl, producerRole) == 0x88);

	// ---- record framing (PROTOCOL.md §4.2) -------------------------------------------------------
	inline constexpr std::uint32_t kRecordAlign = 16;
	inline constexpr std::uint32_t kRecordHeaderBytes = 16;
	inline constexpr std::uint32_t kMaxPayload = 4096;

	struct RecordHeader
	{
		std::uint16_t type;
		std::uint16_t typeVersion;
		std::uint32_t payloadBytes;
		std::uint32_t session;
		std::uint32_t seq;
	};
	static_assert(sizeof(RecordHeader) == kRecordHeaderBytes);
	static_assert(offsetof(RecordHeader, payloadBytes) == 4);
	static_assert(offsetof(RecordHeader, session) == 8);
	static_assert(offsetof(RecordHeader, seq) == 12);

	constexpr std::uint64_t RecordBytes(std::uint64_t a_payloadBytes)
	{
		return (kRecordHeaderBytes + a_payloadBytes + (kRecordAlign - 1)) & ~std::uint64_t(kRecordAlign - 1);
	}
	inline constexpr std::uint64_t kMaxRecordBytes = RecordBytes(kMaxPayload);

	// ---- message types (PROTOCOL.md §7) ----------------------------------------------------------
	enum MsgType : std::uint16_t
	{
		kMsgPad = 0,
		kMsgHello = 1,
		kMsgHeartbeat = 2,
		kMsgPlayerState = 3,
		kMsgBlockSet = 4,
		kMsgBlockBreakRequest = 5,
		kMsgBlockPlaceRequest = 6,
		kMsgLog = 7,
		kMsgRemotePlayerJoin = 8,
		kMsgRemotePlayerState = 9,
		kMsgRemotePlayerLeave = 10,
		kMsgTerrainRequest = 11,
		kMsgTerrainPatch = 12,
		kMsgSessionInfo = 13,
		kMsgCamera = 14,
		kMsgView = 15,
		kMsgInput = 16,
		kMsgOwnerState = 17,
		kMsgBlockRegionRequest = 18,
		kMsgTestPattern = 0x7F00,
	};
	inline constexpr std::uint16_t kTypeVersion1 = 1;

	// §7.1
	inline constexpr std::uint32_t kSoftwareMaxBytes = 40;
	struct HelloMsg
	{
		static constexpr MsgType kType = kMsgHello;
		std::uint16_t versionMajor;
		std::uint16_t versionMinor;
		std::uint32_t role;
		std::uint32_t pid;
		std::uint32_t session;
		std::uint32_t capabilities;
		std::uint16_t softwareBytes;
		std::uint16_t reserved0;
		char          software[kSoftwareMaxBytes];
	};
	static_assert(sizeof(HelloMsg) == 64);
	static_assert(offsetof(HelloMsg, role) == 4);
	static_assert(offsetof(HelloMsg, pid) == 8);
	static_assert(offsetof(HelloMsg, session) == 12);
	static_assert(offsetof(HelloMsg, capabilities) == 16);
	static_assert(offsetof(HelloMsg, softwareBytes) == 20);
	static_assert(offsetof(HelloMsg, software) == 24);

	// §7.2
	struct HeartbeatMsg
	{
		static constexpr MsgType kType = kMsgHeartbeat;
		std::uint64_t sentUs;
		std::uint64_t echoUs;
		std::uint64_t echoHoldUs;
		std::uint32_t counter;
		std::uint32_t reserved0;
	};
	static_assert(sizeof(HeartbeatMsg) == 32);
	static_assert(offsetof(HeartbeatMsg, echoUs) == 8);
	static_assert(offsetof(HeartbeatMsg, echoHoldUs) == 16);
	static_assert(offsetof(HeartbeatMsg, counter) == 24);

	// §7.3
	enum PlayerFlags : std::uint32_t
	{
		kPlayerOnGround = 1u << 0,
		kPlayerTeleport = 1u << 1,
	};
	inline constexpr std::uint32_t kPlayerKnownFlags = kPlayerOnGround | kPlayerTeleport;
	inline constexpr double        kMaxHorizontalCoord = 30000000.0;
	inline constexpr double        kMinY = -2048.0;
	inline constexpr double        kMaxY = 4096.0;
	inline constexpr float         kMaxSpeed = 1000.0f;

	struct PlayerStateMsg
	{
		static constexpr MsgType kType = kMsgPlayerState;
		double        x, y, z;
		float         vx, vy, vz;
		float         yaw, pitch;
		std::uint32_t flags;
		std::uint64_t timeUs;
		std::uint32_t frame;
		std::uint32_t reserved0;
	};
	static_assert(sizeof(PlayerStateMsg) == 64);
	static_assert(offsetof(PlayerStateMsg, y) == 8);
	static_assert(offsetof(PlayerStateMsg, z) == 16);
	static_assert(offsetof(PlayerStateMsg, vx) == 24);
	static_assert(offsetof(PlayerStateMsg, vy) == 28);
	static_assert(offsetof(PlayerStateMsg, vz) == 32);
	static_assert(offsetof(PlayerStateMsg, yaw) == 36);
	static_assert(offsetof(PlayerStateMsg, pitch) == 40);
	static_assert(offsetof(PlayerStateMsg, flags) == 44);
	static_assert(offsetof(PlayerStateMsg, timeUs) == 48);
	static_assert(offsetof(PlayerStateMsg, frame) == 56);

	// §7.4
	inline constexpr std::uint32_t kBlockAir = 0;
	enum BlockSetFlags : std::uint32_t
	{
		kBlockSetEcho = 1u << 0,
		kBlockSetSolid = 1u << 1,   // v1.3: solid in the host game (§7.19)
		kBlockSetRegion = 1u << 2,  // v1.3: answers a BLOCK_REGION_REQUEST
	};
	inline constexpr std::uint32_t kBlockSetKnownFlags = kBlockSetEcho | kBlockSetSolid | kBlockSetRegion;
	struct BlockSetMsg
	{
		static constexpr MsgType kType = kMsgBlockSet;
		std::int32_t  x, y, z;
		std::uint32_t blockId;
		std::uint32_t flags;
		std::uint32_t requestId;
	};
	static_assert(sizeof(BlockSetMsg) == 24);
	static_assert(offsetof(BlockSetMsg, blockId) == 12);
	static_assert(offsetof(BlockSetMsg, flags) == 16);
	static_assert(offsetof(BlockSetMsg, requestId) == 20);

	// §7.5
	inline constexpr std::uint8_t kFaceMax = 5;
	inline constexpr std::uint8_t kFaceUnknown = 0xFF;
	struct BlockBreakRequestMsg
	{
		static constexpr MsgType kType = kMsgBlockBreakRequest;
		std::uint32_t requestId;
		std::int32_t  x, y, z;
		std::uint8_t  face;
		std::uint8_t  reserved0[3];
		std::uint32_t flags;
	};
	static_assert(sizeof(BlockBreakRequestMsg) == 24);
	static_assert(offsetof(BlockBreakRequestMsg, x) == 4);
	static_assert(offsetof(BlockBreakRequestMsg, face) == 16);
	static_assert(offsetof(BlockBreakRequestMsg, flags) == 20);

	struct BlockPlaceRequestMsg
	{
		static constexpr MsgType kType = kMsgBlockPlaceRequest;
		std::uint32_t requestId;
		std::int32_t  x, y, z;
		std::uint8_t  face;
		std::uint8_t  reserved0[3];
		std::uint32_t blockId;  // 0 = whatever the player holds
	};
	static_assert(sizeof(BlockPlaceRequestMsg) == 24);
	static_assert(offsetof(BlockPlaceRequestMsg, face) == 16);
	static_assert(offsetof(BlockPlaceRequestMsg, blockId) == 20);

	// §7.6
	enum LogLevel : std::uint8_t
	{
		kLogTrace = 0,
		kLogDebug = 1,
		kLogInfo = 2,
		kLogWarn = 3,
		kLogError = 4,
	};
	inline constexpr std::uint32_t kLogTextMaxBytes = 256;
	struct LogMsg
	{
		static constexpr MsgType kType = kMsgLog;
		std::uint8_t  level;
		std::uint8_t  reserved0;
		std::uint16_t textBytes;
		std::uint32_t reserved1;
		char          text[kLogTextMaxBytes];
	};
	static_assert(sizeof(LogMsg) == 264);
	static_assert(offsetof(LogMsg, textBytes) == 2);
	static_assert(offsetof(LogMsg, text) == 8);

	// §7.8 (v1.1)
	inline constexpr std::uint32_t kPlayerNameMaxBytes = 32;
	inline constexpr std::uint32_t kUuidBytes = 16;
	struct RemotePlayerJoinMsg
	{
		static constexpr MsgType kType = kMsgRemotePlayerJoin;
		std::uint32_t playerId;
		std::uint32_t flags;
		std::uint8_t  uuid[kUuidBytes];  // most significant byte first
		std::uint16_t nameBytes;
		std::uint8_t  reserved0[6];
		char          name[kPlayerNameMaxBytes];
	};
	static_assert(sizeof(RemotePlayerJoinMsg) == 64);
	static_assert(offsetof(RemotePlayerJoinMsg, flags) == 4);
	static_assert(offsetof(RemotePlayerJoinMsg, uuid) == 8);
	static_assert(offsetof(RemotePlayerJoinMsg, nameBytes) == 24);
	static_assert(offsetof(RemotePlayerJoinMsg, name) == 32);

	// §7.9 (v1.1)
	enum RemotePlayerFlags : std::uint32_t
	{
		kRemoteOnGround = 1u << 0,
		kRemoteCrouching = 1u << 1,
		kRemoteSprinting = 1u << 2,
		kRemoteSwimming = 1u << 3,
		kRemoteGliding = 1u << 4,
		kRemoteFlying = 1u << 5,
		kRemoteInWater = 1u << 6,
		kRemoteSwing = 1u << 7,
		kRemoteSleeping = 1u << 8,
		kRemoteRiding = 1u << 9,
	};
	inline constexpr std::uint32_t kRemoteKnownFlags = (1u << 10) - 1;
	inline constexpr std::uint8_t  kGameModeMax = 3;  // 0 survival, 1 creative, 2 adventure, 3 spectator
	struct RemotePlayerStateMsg
	{
		static constexpr MsgType kType = kMsgRemotePlayerState;
		std::uint32_t playerId;
		std::uint32_t flags;
		double        x, y, z;
		float         vx, vy, vz;
		float         yaw, pitch, bodyYaw;
		std::uint32_t tick;
		std::uint8_t  gameMode;
		std::uint8_t  health;
		std::uint8_t  reserved0[2];
	};
	static_assert(sizeof(RemotePlayerStateMsg) == 64);
	static_assert(offsetof(RemotePlayerStateMsg, x) == 8);
	static_assert(offsetof(RemotePlayerStateMsg, vx) == 32);
	static_assert(offsetof(RemotePlayerStateMsg, yaw) == 44);
	static_assert(offsetof(RemotePlayerStateMsg, pitch) == 48);
	static_assert(offsetof(RemotePlayerStateMsg, bodyYaw) == 52);
	static_assert(offsetof(RemotePlayerStateMsg, tick) == 56);
	static_assert(offsetof(RemotePlayerStateMsg, gameMode) == 60);
	static_assert(offsetof(RemotePlayerStateMsg, health) == 61);

	// §7.10 (v1.1)
	enum LeaveReason : std::uint32_t
	{
		kLeaveLeft = 0,
		kLeaveOtherDimension = 1,
		kLeaveReset = 2,
	};
	struct RemotePlayerLeaveMsg
	{
		static constexpr MsgType kType = kMsgRemotePlayerLeave;
		std::uint32_t playerId;
		std::uint32_t reason;  // LeaveReason
	};
	static_assert(sizeof(RemotePlayerLeaveMsg) == 8);

	// §7.11 (v1.1)
	inline constexpr std::int32_t  kMaxChunkCoord = 1875000;  // kMaxHorizontalCoord / 16
	inline constexpr std::uint32_t kTerrainMaxInFlight = 32;
	inline constexpr std::uint64_t kTerrainRetryMs = 5000;
	struct TerrainRequestMsg
	{
		static constexpr MsgType kType = kMsgTerrainRequest;
		std::int32_t  chunkX, chunkZ;
		std::uint32_t requestId;
		std::uint16_t distance;
		std::uint16_t reserved0;
	};
	static_assert(sizeof(TerrainRequestMsg) == 16);
	static_assert(offsetof(TerrainRequestMsg, requestId) == 8);
	static_assert(offsetof(TerrainRequestMsg, distance) == 12);

	// §7.12 (v1.1)
	inline constexpr std::uint32_t kChunkColumns = 256;  // 16 x 16, index = localZ * 16 + localX
	inline constexpr std::int16_t  kNoGround = -32768;
	inline constexpr std::int16_t  kNoWater = -32768;
	enum TerrainMaterial : std::uint8_t
	{
		kMatUnknown = 0,
		kMatGrass = 1,
		kMatDirt = 2,
		kMatSand = 3,
		kMatRock = 4,
		kMatRoad = 5,
		kMatPavement = 6,
		kMatGravel = 7,
		kMatSnow = 8,
		kMatWood = 9,
		kMatMetal = 10,
		kMatBuilding = 11,
		kMatMud = 12,
	};
	struct TerrainPatchMsg
	{
		static constexpr MsgType kType = kMsgTerrainPatch;
		std::int32_t  chunkX, chunkZ;
		std::uint32_t requestId;  // 0 = unsolicited
		std::uint32_t flags;
		std::int16_t  groundY[kChunkColumns];
		std::int16_t  waterY[kChunkColumns];
		std::uint8_t  material[kChunkColumns];
	};
	static_assert(sizeof(TerrainPatchMsg) == 1296);
	static_assert(offsetof(TerrainPatchMsg, requestId) == 8);
	static_assert(offsetof(TerrainPatchMsg, groundY) == 16);
	static_assert(offsetof(TerrainPatchMsg, waterY) == 528);
	static_assert(offsetof(TerrainPatchMsg, material) == 1040);

	// §7.13 (v1.1)
	enum SessionFlags : std::uint32_t
	{
		kSessionOpen = 1u << 0,
		kSessionAuth = 1u << 1,
		kSessionWhitelist = 1u << 2,
	};
	inline constexpr std::uint32_t kSessionKnownFlags = kSessionOpen | kSessionAuth | kSessionWhitelist;
	inline constexpr std::uint32_t kAddressMaxBytes = 64;
	struct SessionInfoMsg
	{
		static constexpr MsgType kType = kMsgSessionInfo;
		std::uint32_t flags;
		std::uint16_t port;
		std::uint16_t friends;
		std::uint16_t maxPlayers;
		std::uint8_t  gameMode;
		std::uint8_t  reserved0;
		std::uint16_t addressBytes;
		std::uint16_t reserved1;
		char          address[kAddressMaxBytes];
		std::uint8_t  reserved2[16];
	};
	static_assert(sizeof(SessionInfoMsg) == 96);
	static_assert(offsetof(SessionInfoMsg, port) == 4);
	static_assert(offsetof(SessionInfoMsg, friends) == 6);
	static_assert(offsetof(SessionInfoMsg, maxPlayers) == 8);
	static_assert(offsetof(SessionInfoMsg, gameMode) == 10);
	static_assert(offsetof(SessionInfoMsg, addressBytes) == 12);
	static_assert(offsetof(SessionInfoMsg, address) == 16);

	// §7.15 (v1.2)
	enum CameraFlags : std::uint32_t
	{
		kCameraFirstPerson = 1u << 0,
		kCameraPassthrough = 1u << 1,  // the host composites MC's frame export (§11): render the owner's view for it
		kCameraInVehicle = 1u << 2,
	};
	inline constexpr std::uint32_t kCameraKnownFlags = kCameraFirstPerson | kCameraPassthrough | kCameraInVehicle;
	inline constexpr float         kMinFov = 1.0f;
	inline constexpr float         kMaxFov = 179.0f;
	struct CameraMsg
	{
		static constexpr MsgType kType = kMsgCamera;
		std::uint64_t frame;
		std::uint64_t timeUs;
		double        x, y, z;
		float         yaw, pitch, roll, fovY;
		double        feetX, feetY, feetZ;
		float         bodyYaw;
		std::uint32_t flags;
		float         nearClip, farClip;
	};
	static_assert(sizeof(CameraMsg) == 96);
	static_assert(offsetof(CameraMsg, timeUs) == 8);
	static_assert(offsetof(CameraMsg, x) == 16);
	static_assert(offsetof(CameraMsg, yaw) == 40);
	static_assert(offsetof(CameraMsg, fovY) == 52);
	static_assert(offsetof(CameraMsg, feetX) == 56);
	static_assert(offsetof(CameraMsg, bodyYaw) == 80);
	static_assert(offsetof(CameraMsg, flags) == 84);
	static_assert(offsetof(CameraMsg, nearClip) == 88);

	// §7.16 (v1.2)
	inline constexpr std::uint32_t kViewMinSide = 64;
	inline constexpr std::uint32_t kViewMaxWidth = 3840;
	inline constexpr std::uint32_t kViewMaxHeight = 2160;
	inline constexpr std::uint64_t kViewMaxPixels = 2560ull * 1440;
	inline constexpr std::uint32_t kHostMaxSide = 16384;
	struct ViewMsg
	{
		static constexpr MsgType kType = kMsgView;
		std::uint32_t width, height;
		std::uint32_t hostWidth, hostHeight;
	};
	static_assert(sizeof(ViewMsg) == 16);
	static_assert(offsetof(ViewMsg, hostWidth) == 8);

	// §7.17 (v1.2)
	enum InputKind : std::uint8_t
	{
		kInputButton = 1,
		kInputSlot = 2,
		kInputScroll = 3,
		kInputCursor = 4,  // v1.3
	};
	enum InputButton : std::uint8_t
	{
		kButtonAttack = 1,
		kButtonUse = 2,
		kButtonPick = 3,
		kButtonDrop = 4,
		kButtonInventory = 5,
		kButtonSwapHands = 6,
		kButtonCloseScreen = 7,
	};
	inline constexpr std::int8_t kHotbarSlots = 9;
	struct InputMsg
	{
		static constexpr MsgType kType = kMsgInput;
		std::uint8_t  kind;
		std::uint8_t  button;
		std::uint8_t  down;
		std::int8_t   value;
		std::uint32_t cursor;  // CURSOR: x in the low 16 bits, y in the high, 0..65535 across the picture
	};
	static_assert(sizeof(InputMsg) == 8);
	static_assert(offsetof(InputMsg, value) == 3);

	// §7.18 (v1.2)
	enum HeldKind : std::uint8_t
	{
		kHeldEmpty = 0,
		kHeldSword = 1,
		kHeldAxe = 2,
		kHeldPickaxe = 3,
		kHeldShovel = 4,
		kHeldHoe = 5,
		kHeldBlock = 6,
		kHeldOther = 7,
	};
	enum OwnerFlags : std::uint32_t
	{
		kOwnerDead = 1u << 0,
		kOwnerScreenOpen = 1u << 1,
	};
	inline constexpr std::uint32_t kOwnerKnownFlags = kOwnerDead | kOwnerScreenOpen;
	inline constexpr std::uint8_t  kMaxFood = 20;
	inline constexpr float         kMaxAttackDamage = 1000.0f;
	struct OwnerStateMsg
	{
		static constexpr MsgType kType = kMsgOwnerState;
		std::uint8_t  held;
		std::uint8_t  health;
		std::uint8_t  food;
		std::uint8_t  gameMode;
		float         attackDamage;
		float         attackCharge;
		std::uint32_t flags;
	};
	static_assert(sizeof(OwnerStateMsg) == 16);
	static_assert(offsetof(OwnerStateMsg, attackDamage) == 4);
	static_assert(offsetof(OwnerStateMsg, flags) == 12);

	// §7.19 (v1.3)
	struct BlockRegionRequestMsg
	{
		static constexpr MsgType kType = kMsgBlockRegionRequest;
		std::int32_t  chunkX, chunkZ;
		std::uint32_t requestId;
		std::uint32_t reserved0;
	};
	static_assert(sizeof(BlockRegionRequestMsg) == 16);
	static_assert(offsetof(BlockRegionRequestMsg, requestId) == 8);

	// §11 (v1.2): the frame mapping MC writes for the passthrough
	inline constexpr const wchar_t* kFrameMappingName = L"Local\\CraftV_Frame_v1";
	inline constexpr std::uint32_t  kFrameMagic = 0x52465643;  // bytes 43 56 46 52 = "CVFR"
	inline constexpr std::uint32_t  kFrameVersion = 1;
	inline constexpr std::uint32_t  kFrameHeaderBytes = 4096;
	inline constexpr std::uint32_t  kFrameSlotDescOffset = 256;
	inline constexpr std::uint32_t  kFrameSlotDescBytes = 128;
	inline constexpr std::uint32_t  kFrameSlots = 3;
	inline constexpr std::uint64_t  kFrameLayerMaxBytes = kViewMaxPixels * 4;
	inline constexpr std::uint64_t  kFrameSlotStride = kFrameLayerMaxBytes * 3;
	enum FrameFlags : std::uint32_t
	{
		kFrameDepthZeroToOne = 1u << 0,
		kFrameBottomUp = 1u << 1,
		kFrameReversedZ = 1u << 2,
	};

	// §7.14 (test only)
	inline constexpr std::uint32_t kTestPatternFixedBytes = 16;
	inline constexpr std::uint32_t kTestPatternMaxFill = 256;
	inline constexpr std::uint32_t kTestPatternFillModulus = kTestPatternMaxFill + 1;
	struct TestPatternFixed
	{
		std::uint64_t index;
		std::uint32_t fillBytes;
		std::uint32_t checksum;  // FNV-1a-32 of the fill
	};
	static_assert(sizeof(TestPatternFixed) == kTestPatternFixedBytes);

	// ---- timing (PROTOCOL.md §5.4) ---------------------------------------------------------------
	inline constexpr std::uint64_t kHeartbeatPeriodMs = 100;
	inline constexpr std::uint64_t kHostTimeoutMs = 10000;
	inline constexpr std::uint64_t kMcTimeoutMs = 3000;
	inline constexpr std::uint64_t kHeartbeatMsgPeriodMs = 500;
	inline constexpr std::uint64_t kInitTimeoutMs = 2000;
	inline constexpr std::uint64_t kRetryMs = 1000;
	inline constexpr std::uint64_t kInvalidRetryMs = 5000;
	inline constexpr std::uint64_t kMaxDrainBytesPerTick = 256ull * 1024;
}
