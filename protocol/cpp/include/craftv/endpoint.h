// One side of the CraftV link: mapping lifecycle, sessions, heartbeats, link state, HELLO and
// HEARTBEAT handling, typed sends (PROTOCOL.md §5, §7). The mock host uses it as role kHost, and
// the RDR2 ASI will use exactly the same class.
//
// Threading: one thread owns an Endpoint (Tick, Drain and Send from that thread only).
// Hot path (Tick/Drain/Send): no heap allocation, no blocking calls.
#pragma once

#include "craftv/codec.h"
#include "craftv/mapping.h"
#include "craftv/protocol.h"
#include "craftv/ring.h"

#include <cstdint>

namespace craftv
{
	enum class LinkState
	{
		kDetached,   // no mapping
		kWaiting,    // mapping open, not initialised/validated yet
		kAttached,   // attached, peer absent
		kConnected,  // peer attached and beating
		kStale,      // peer attached but its heartbeat stopped
	};
	const char* ToString(LinkState a_state);

	// Bits returned by Endpoint::TakeEvents().
	enum LinkEvent : std::uint32_t
	{
		kEvAttached = 1u << 0,         // we attached with a new session
		kEvConnected = 1u << 1,        // CONNECTED to a peer session for the first time
		kEvResumed = 1u << 2,          // STALE -> CONNECTED, same peer session (peer resumed)
		kEvStale = 1u << 3,            // peer heartbeat timed out
		kEvPeerDetached = 1u << 4,     // peer shut down cleanly
		kEvPeerRestarted = 1u << 5,    // a new peer session replaced a known one: resync
		kEvPeerHello = 1u << 6,        // HELLO received (see Peer())
		kEvVersionMismatch = 1u << 7,  // peer HELLO has another major version
		kEvDetached = 1u << 8,         // we detached / lost the mapping
		kEvCorrupt = 1u << 9,          // incoming ring was corrupt; its backlog was dropped
	};

	using LogFn = void (*)(void* a_user, proto::LogLevel a_level, const char* a_text);

	struct EndpointConfig
	{
		proto::Role    role = proto::Role::kHost;
		const wchar_t* mappingName = proto::kDefaultMappingName;  // must outlive the endpoint
		std::uint64_t  peerTimeoutMs = 0;                         // 0: host uses kMcTimeoutMs, MC uses kHostTimeoutMs
		const char*    software = "CraftV";                       // goes into HELLO (truncated to 40 bytes)
		LogFn          log = nullptr;
		void*          logUser = nullptr;
	};

	struct PeerInfo
	{
		std::uint32_t   session = 0;
		std::uint32_t   pid = 0;
		bool            helloSeen = false;
		proto::HelloMsg hello{};
		std::uint64_t   rttUs = 0;
		bool            rttValid = false;
	};

	class Endpoint
	{
	public:
		explicit Endpoint(const EndpointConfig& a_config);
		Endpoint(const Endpoint&) = delete;
		Endpoint& operator=(const Endpoint&) = delete;
		~Endpoint();

		// Call at least every HEARTBEAT_PERIOD_MS (100 ms); every frame is normal.
		void Tick(std::uint64_t a_nowMs, std::uint64_t a_nowUs);

		// Reads up to about a_maxBytes of incoming records. HELLO and HEARTBEAT are handled here;
		// every other accepted record goes to a_onMessage(const proto::RecordHeader&, const std::uint8_t* payload).
		// The payload points into shared memory: decode it with codec::Decode (one memcpy).
		template <class F>
		void Drain(std::uint64_t a_nowUs, std::uint64_t a_maxBytes, F&& a_onMessage)
		{
			if (!attached_) {
				return;
			}
			const auto status = rx_.Drain(&Theirs().session, a_maxBytes, rxStats_, [&](const proto::RecordHeader& a_header, const std::uint8_t* a_payload) {
				if (!codec::AllowedFrom(a_header.type, peerRole_)) {
					++rxStats_.malformed;
					return;
				}
				switch (a_header.type) {
				case proto::kMsgHello:
					HandleHello(a_header, a_payload);
					return;
				case proto::kMsgHeartbeat:
					HandleHeartbeat(a_header, a_payload, a_nowUs);
					return;
				case proto::kMsgPlayerState:
				case proto::kMsgBlockSet:
				case proto::kMsgBlockBreakRequest:
				case proto::kMsgBlockPlaceRequest:
				case proto::kMsgLog:
				case proto::kMsgRemotePlayerJoin:
				case proto::kMsgRemotePlayerState:
				case proto::kMsgRemotePlayerLeave:
				case proto::kMsgTerrainRequest:
				case proto::kMsgTerrainPatch:
				case proto::kMsgSessionInfo:
				case proto::kMsgCamera:
				case proto::kMsgView:
				case proto::kMsgInput:
				case proto::kMsgOwnerState:
				case proto::kMsgBlockRegionRequest:
				case proto::kMsgWorldEvent:
				case proto::kMsgTestPattern:
					if (!versionMismatch_) {
						a_onMessage(a_header, a_payload);
					}
					return;
				default:
					++rxStats_.unknown;
					return;
				}
			});
			if (status == DrainStatus::kCorrupt) {
				OnCorrupt();
			}
		}

		// Sends one message. HELLO/HEARTBEAT/LOG need only an attachment; everything else needs
		// CONNECTED (PROTOCOL.md §5.3). False = not sent (not connected, ring full, too large).
		bool Send(std::uint16_t a_type, const void* a_payload, std::uint32_t a_bytes);

		template <class T>
		bool Send(const T& a_msg)
		{
			return Send(T::kType, &a_msg, sizeof(T));
		}

		bool SendLog(proto::LogLevel a_level, const char* a_text);

		// Clean shutdown (§5.3): clear ATTACHED, keep the session id, unmap.
		void Detach();

		// Test hook ("kill-link"): stop incrementing the heartbeat counter while true.
		void SetHeartbeatSuspended(bool a_suspended) { heartbeatSuspended_ = a_suspended; }
		bool HeartbeatSuspended() const { return heartbeatSuspended_; }
		void SetInGame(bool a_inGame);

		// Count a record the caller couldn't decode (codec::Decode returned false).
		void CountMalformed() { ++rxStats_.malformed; }

		LinkState        State() const { return state_; }
		bool             Connected() const { return state_ == LinkState::kConnected; }
		std::uint32_t    TakeEvents();
		std::uint32_t    Session() const { return session_; }
		const PeerInfo&  Peer() const { return peer_; }
		const RingStats& TxStats() const { return txStats_; }
		const RingStats& RxStats() const { return rxStats_; }
		std::uint64_t    DroppedNotConnected() const { return droppedNotConnected_; }
		std::uint64_t    TxBacklog() const { return tx_.Backlog(); }
		std::uint64_t    RxPending() const { return rx_.Pending(); }
		bool             WasCreator() const { return mapping_.WasCreator(); }
		proto::Role      GetRole() const { return config_.role; }

	private:
		proto::SideBlock& Mine();
		proto::SideBlock& Theirs();
		void              TryOpen(std::uint64_t a_nowMs, std::uint64_t a_nowUs);
		void              PollWaiting(std::uint64_t a_nowMs, std::uint64_t a_nowUs);
		void              Attach(std::uint64_t a_nowMs, std::uint64_t a_nowUs);
		void              UpdatePeer(std::uint64_t a_nowMs);
		void              SendControl(std::uint64_t a_nowMs, std::uint64_t a_nowUs);
		void              HandleHello(const proto::RecordHeader& a_header, const std::uint8_t* a_payload);
		void              HandleHeartbeat(const proto::RecordHeader& a_header, const std::uint8_t* a_payload, std::uint64_t a_nowUs);
		void              OnCorrupt();
		void              CloseMapping(std::uint64_t a_retryAtMs);
		void              Log(proto::LogLevel a_level, const char* a_fmt, ...);
		void              LogOnce(char* a_last, std::size_t a_cap, proto::LogLevel a_level, const char* a_text);

		EndpointConfig config_;
		proto::Role    peerRole_;
		std::uint64_t  peerTimeoutMs_;
		std::uint32_t  pid_;

		SharedMapping mapping_;
		RingProducer  tx_;
		RingConsumer  rx_;
		RingStats     txStats_{};
		RingStats     rxStats_{};

		LinkState     state_ = LinkState::kDetached;
		bool          attached_ = false;
		std::uint32_t session_ = 0;
		std::uint32_t txSeq_ = 0;
		std::uint32_t events_ = 0;
		bool          heartbeatSuspended_ = false;
		bool          inGame_ = false;
		bool          helloPending_ = false;
		bool          versionMismatch_ = false;
		std::uint64_t nextOpenMs_ = 0;
		std::uint64_t waitingSinceMs_ = 0;
		std::uint64_t nextHeartbeatMsgMs_ = 0;
		std::uint64_t droppedNotConnected_ = 0;
		std::uint64_t lastCorruptLogMs_ = 0;

		PeerInfo      peer_{};
		std::uint64_t peerBeat_ = 0;
		std::uint64_t peerBeatChangedMs_ = 0;
		bool          peerBeatObserved_ = false;
		bool          peerEverConnected_ = false;
		std::uint64_t peerHbSentUs_ = 0;  // sentUs of the newest HEARTBEAT from the peer
		std::uint64_t peerHbRecvUs_ = 0;  // when we received it

		char lastOpenError_[160] = {};
		char lastInvalidError_[160] = {};
	};
}
