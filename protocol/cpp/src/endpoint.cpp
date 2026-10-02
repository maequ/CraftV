#include "redcraft/endpoint.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace redcraft
{
	using namespace redcraft::proto;

	namespace
	{
		constexpr std::size_t   kLogLineBytes = 320;
		constexpr std::uint64_t kCorruptLogIntervalMs = 5000;

		bool ProcessAlive(std::uint32_t a_pid)
		{
			if (a_pid == 0) {
				return false;
			}
			HANDLE process = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, a_pid);
			if (!process) {
				// Access denied still means it exists (e.g. an elevated peer of my role).
				return ::GetLastError() == ERROR_ACCESS_DENIED;
			}
			DWORD code = 0;
			const bool alive = ::GetExitCodeProcess(process, &code) && code == STILL_ACTIVE;
			::CloseHandle(process);
			return alive;
		}

		const char* RoleName(Role a_role)
		{
			return a_role == Role::kHost ? "host" : a_role == Role::kMc ? "minecraft" : "?";
		}
	}

	const char* ToString(LinkState a_state)
	{
		switch (a_state) {
		case LinkState::kDetached:
			return "DETACHED";
		case LinkState::kWaiting:
			return "WAITING";
		case LinkState::kAttached:
			return "ATTACHED";
		case LinkState::kConnected:
			return "CONNECTED";
		case LinkState::kStale:
			return "STALE";
		}
		return "?";
	}

	Endpoint::Endpoint(const EndpointConfig& a_config) :
		config_(a_config),
		peerRole_(a_config.role == Role::kHost ? Role::kMc : Role::kHost),
		peerTimeoutMs_(a_config.peerTimeoutMs ? a_config.peerTimeoutMs : (a_config.role == Role::kHost ? kMcTimeoutMs : kHostTimeoutMs)),
		pid_(::GetCurrentProcessId())
	{
	}

	Endpoint::~Endpoint()
	{
		Detach();
	}

	SideBlock& Endpoint::Mine()
	{
		auto* header = mapping_.HeaderPtr();
		return config_.role == Role::kHost ? header->host : header->mc;
	}

	SideBlock& Endpoint::Theirs()
	{
		auto* header = mapping_.HeaderPtr();
		return config_.role == Role::kHost ? header->mc : header->host;
	}

	void Endpoint::Log(LogLevel a_level, const char* a_fmt, ...)
	{
		if (!config_.log) {
			return;
		}
		char    line[kLogLineBytes];
		va_list args;
		va_start(args, a_fmt);
		std::vsnprintf(line, sizeof(line), a_fmt, args);
		va_end(args);
		config_.log(config_.logUser, a_level, line);
	}

	void Endpoint::LogOnce(char* a_last, std::size_t a_cap, LogLevel a_level, const char* a_text)
	{
		if (std::strncmp(a_last, a_text, a_cap) != 0) {
			std::snprintf(a_last, a_cap, "%s", a_text);
			Log(a_level, "%s", a_text);
		}
	}

	void Endpoint::Tick(std::uint64_t a_nowMs, std::uint64_t a_nowUs)
	{
		if (state_ == LinkState::kDetached && a_nowMs >= nextOpenMs_) {
			TryOpen(a_nowMs, a_nowUs);
		} else if (state_ == LinkState::kWaiting) {
			PollWaiting(a_nowMs, a_nowUs);
		}
		if (!attached_) {
			return;
		}
		auto& mine = Mine();
		if (!heartbeatSuspended_) {
			detail::Atomic(mine.heartbeatUs).store(a_nowUs, std::memory_order_relaxed);
			detail::Atomic(mine.heartbeat).fetch_add(1, std::memory_order_release);
		}
		UpdatePeer(a_nowMs);
		SendControl(a_nowMs, a_nowUs);
	}

	void Endpoint::TryOpen(std::uint64_t a_nowMs, std::uint64_t a_nowUs)
	{
		char err[160];
		if (!mapping_.CreateOrOpen(config_.mappingName, config_.role, err, sizeof(err))) {
			LogOnce(lastOpenError_, sizeof(lastOpenError_), kLogWarn, err);
			nextOpenMs_ = a_nowMs + kRetryMs;
			return;
		}
		lastOpenError_[0] = '\0';
		Log(kLogInfo, "shared memory %s (%llu KiB)", mapping_.WasCreator() ? "created" : "opened",
			static_cast<unsigned long long>(mapping_.ViewBytes() / 1024));
		state_ = LinkState::kWaiting;
		waitingSinceMs_ = a_nowMs;
		PollWaiting(a_nowMs, a_nowUs);
	}

	void Endpoint::PollWaiting(std::uint64_t a_nowMs, std::uint64_t a_nowUs)
	{
		char err[160];
		switch (mapping_.PollReady(err, sizeof(err))) {
		case ReadyResult::kReady:
			lastInvalidError_[0] = '\0';
			Attach(a_nowMs, a_nowUs);
			return;
		case ReadyResult::kNotYet:
			if (a_nowMs - waitingSinceMs_ > kInitTimeoutMs) {
				Log(kLogWarn, "shared memory never initialised by its creator; initialising it");
				mapping_.ForceInitialize(config_.role);
			}
			return;
		case ReadyResult::kInvalid:
			LogOnce(lastInvalidError_, sizeof(lastInvalidError_), kLogError, err);
			CloseMapping(a_nowMs + kInvalidRetryMs);
			return;
		}
	}

	void Endpoint::Attach(std::uint64_t a_nowMs, std::uint64_t a_nowUs)
	{
		auto&      mine = Mine();
		const auto mineState = detail::Atomic(mine.state).load(std::memory_order_acquire);
		const auto minePid = detail::Atomic(mine.pid).load(std::memory_order_acquire);
		if ((mineState & kSideAttached) && minePid != pid_ && ProcessAlive(minePid)) {
			char text[160];
			std::snprintf(text, sizeof(text), "another %s (pid %u) is already attached; waiting", RoleName(config_.role), minePid);
			LogOnce(lastInvalidError_, sizeof(lastInvalidError_), kLogError, text);
			CloseMapping(a_nowMs + kInvalidRetryMs);
			return;
		}
		const Layout& layout = mapping_.GetLayout();
		const RingLocation& txLoc = config_.role == Role::kHost ? layout.hostToMc : layout.mcToHost;
		const RingLocation& rxLoc = config_.role == Role::kHost ? layout.mcToHost : layout.hostToMc;
		// §5.2: drop the stale backlog of the ring we consume, continue the ring we produce.
		rx_.Bind(rxLoc.control, rxLoc.data, rxLoc.dataBytes);
		rx_.DiscardBacklog();
		tx_.Bind(txLoc.control, txLoc.data, txLoc.dataBytes);

		const auto previous = detail::Atomic(mine.session).load(std::memory_order_acquire);
		session_ = previous + 1;
		if (session_ == 0) {
			session_ = 1;
		}
		detail::Atomic(mine.pid).store(pid_, std::memory_order_relaxed);
		detail::Atomic(mine.heartbeatUs).store(a_nowUs, std::memory_order_relaxed);
		detail::Atomic(mine.heartbeat).fetch_add(1, std::memory_order_relaxed);
		detail::Atomic(mine.state).store(kSideAttached | (inGame_ ? kSideInGame : 0u), std::memory_order_release);
		detail::Atomic(mine.session).store(session_, std::memory_order_release);

		txSeq_ = 0;
		attached_ = true;
		state_ = LinkState::kAttached;
		helloPending_ = true;
		versionMismatch_ = false;
		peer_ = {};
		peerBeatObserved_ = false;
		peerEverConnected_ = false;
		peerHbSentUs_ = 0;
		events_ |= kEvAttached;
		Log(kLogInfo, "attached as %s, session %u", RoleName(config_.role), session_);
	}

	void Endpoint::UpdatePeer(std::uint64_t a_nowMs)
	{
		auto&      theirs = Theirs();
		const auto peerSession = detail::Atomic(theirs.session).load(std::memory_order_acquire);
		const auto peerState = detail::Atomic(theirs.state).load(std::memory_order_acquire);
		const auto peerBeat = detail::Atomic(theirs.heartbeat).load(std::memory_order_acquire);

		if (peerSession != peer_.session) {
			const bool wasKnown = peer_.session != 0;
			// The new peer's HELLO may already have been drained (it attaches and sends HELLO in the
			// same tick, and Drain reads its session straight from the header). Keep it.
			const bool     keepHello = peer_.helloSeen && peer_.hello.session == peerSession;
			const HelloMsg hello = peer_.hello;
			peer_ = {};
			peer_.session = peerSession;
			if (keepHello) {
				peer_.helloSeen = true;
				peer_.hello = hello;
			}
			peer_.pid = detail::Atomic(theirs.pid).load(std::memory_order_acquire);
			peerBeat_ = peerBeat;
			peerBeatChangedMs_ = a_nowMs;
			peerBeatObserved_ = false;
			peerEverConnected_ = false;
			peerHbSentUs_ = 0;
			versionMismatch_ = false;
			if (peerSession != 0) {
				helloPending_ = true;
				if (wasKnown) {
					events_ |= kEvPeerRestarted;
					Log(kLogInfo, "%s restarted (new session %u, pid %u)", RoleName(peerRole_), peerSession, peer_.pid);
				}
			}
		} else if (peerBeat != peerBeat_) {
			peerBeat_ = peerBeat;
			peerBeatChangedMs_ = a_nowMs;
			peerBeatObserved_ = true;
		}

		LinkState next;
		if (peerSession == 0 || !(peerState & kSideAttached)) {
			next = LinkState::kAttached;
		} else if (a_nowMs - peerBeatChangedMs_ > peerTimeoutMs_) {
			next = LinkState::kStale;
		} else if (peerBeatObserved_) {
			next = LinkState::kConnected;
		} else {
			next = LinkState::kAttached;  // just appeared; wait for its first beat
		}
		if (next == state_) {
			return;
		}
		const LinkState previous = state_;
		state_ = next;
		switch (next) {
		case LinkState::kConnected:
			events_ |= peerEverConnected_ ? kEvResumed : kEvConnected;
			Log(kLogInfo, "link %s with %s (session %u, pid %u)", peerEverConnected_ ? "resumed" : "up", RoleName(peerRole_), peer_.session, peer_.pid);
			peerEverConnected_ = true;
			nextHeartbeatMsgMs_ = a_nowMs;
			break;
		case LinkState::kStale:
			events_ |= kEvStale;
			Log(kLogWarn, "link stale: no heartbeat from %s for %llu ms", RoleName(peerRole_),
				static_cast<unsigned long long>(a_nowMs - peerBeatChangedMs_));
			break;
		case LinkState::kAttached:
			if (previous == LinkState::kConnected || previous == LinkState::kStale) {
				events_ |= kEvPeerDetached;
				Log(kLogInfo, "%s detached", RoleName(peerRole_));
			}
			break;
		default:
			break;
		}
	}

	void Endpoint::SendControl(std::uint64_t a_nowMs, std::uint64_t a_nowUs)
	{
		// HELLO only once the peer is attached: an earlier one would be discarded by its attach.
		if (helloPending_ && peer_.session != 0) {
			const HelloMsg hello = codec::MakeHello(config_.role, pid_, session_, config_.software);
			if (Send(hello)) {
				helloPending_ = false;
			}
		}
		if (state_ == LinkState::kConnected && !heartbeatSuspended_ && a_nowMs >= nextHeartbeatMsgMs_) {
			HeartbeatMsg hb{};
			hb.sentUs = a_nowUs;
			hb.echoUs = peerHbSentUs_;
			hb.echoHoldUs = peerHbSentUs_ ? a_nowUs - peerHbRecvUs_ : 0;
			hb.counter = static_cast<std::uint32_t>(detail::Atomic(Mine().heartbeat).load(std::memory_order_relaxed));
			Send(hb);
			nextHeartbeatMsgMs_ = a_nowMs + kHeartbeatMsgPeriodMs;
		}
	}

	bool Endpoint::Send(std::uint16_t a_type, const void* a_payload, std::uint32_t a_bytes)
	{
		if (!attached_) {
			++droppedNotConnected_;
			return false;
		}
		const bool control = a_type == kMsgHello || a_type == kMsgHeartbeat || a_type == kMsgLog;
		if (!control && (state_ != LinkState::kConnected || versionMismatch_)) {
			++droppedNotConnected_;
			return false;
		}
		const auto result = tx_.Push(a_type, kTypeVersion1, session_, txSeq_ + 1, a_payload, a_bytes, txStats_);
		if (result != PushResult::kOk) {
			return false;
		}
		++txSeq_;
		return true;
	}

	bool Endpoint::SendLog(LogLevel a_level, const char* a_text)
	{
		const LogMsg msg = codec::MakeLog(a_level, a_text);
		return Send(msg);
	}

	void Endpoint::SetInGame(bool a_inGame)
	{
		inGame_ = a_inGame;
		if (attached_) {
			detail::Atomic(Mine().state).store(kSideAttached | (inGame_ ? kSideInGame : 0u), std::memory_order_release);
		}
	}

	void Endpoint::HandleHello(const RecordHeader& a_header, const std::uint8_t* a_payload)
	{
		HelloMsg hello;
		if (!codec::Decode(a_header, a_payload, hello) || hello.role != static_cast<std::uint32_t>(peerRole_)) {
			++rxStats_.malformed;
			return;
		}
		peer_.helloSeen = true;
		peer_.hello = hello;
		events_ |= kEvPeerHello;
		char software[kSoftwareMaxBytes + 1] = {};
		std::memcpy(software, hello.software, hello.softwareBytes);
		if (hello.versionMajor != kVersionMajor) {
			versionMismatch_ = true;
			events_ |= kEvVersionMismatch;
			Log(kLogError, "HELLO from %s '%s' speaks protocol %u.%u; we speak %u.%u: ignoring its messages", RoleName(peerRole_), software,
				hello.versionMajor, hello.versionMinor, kVersionMajor, kVersionMinor);
			return;
		}
		Log(kLogInfo, "HELLO from %s '%s' (protocol %u.%u, pid %u, session %u)", RoleName(peerRole_), software, hello.versionMajor,
			hello.versionMinor, hello.pid, hello.session);
	}

	void Endpoint::HandleHeartbeat(const RecordHeader& a_header, const std::uint8_t* a_payload, std::uint64_t a_nowUs)
	{
		HeartbeatMsg hb;
		if (!codec::Decode(a_header, a_payload, hb)) {
			++rxStats_.malformed;
			return;
		}
		peerHbSentUs_ = hb.sentUs;
		peerHbRecvUs_ = a_nowUs;
		if (hb.echoUs != 0 && a_nowUs >= hb.echoUs + hb.echoHoldUs) {
			peer_.rttUs = a_nowUs - hb.echoUs - hb.echoHoldUs;
			peer_.rttValid = true;
		}
	}

	void Endpoint::OnCorrupt()
	{
		events_ |= kEvCorrupt;
		const auto now = ::GetTickCount64();
		if (now - lastCorruptLogMs_ > kCorruptLogIntervalMs) {
			lastCorruptLogMs_ = now;
			Log(kLogError, "incoming ring from %s was corrupt; dropped its backlog (%llu corruptions so far)", RoleName(peerRole_),
				static_cast<unsigned long long>(rxStats_.corrupt));
		}
	}

	std::uint32_t Endpoint::TakeEvents()
	{
		const auto events = events_;
		events_ = 0;
		return events;
	}

	void Endpoint::CloseMapping(std::uint64_t a_retryAtMs)
	{
		tx_.Unbind();
		rx_.Unbind();
		mapping_.Close();
		attached_ = false;
		state_ = LinkState::kDetached;
		nextOpenMs_ = a_retryAtMs;
	}

	void Endpoint::Detach()
	{
		if (attached_) {
			auto& mine = Mine();
			const auto st = detail::Atomic(mine.state).load(std::memory_order_relaxed);
			detail::Atomic(mine.state).store(st & ~std::uint32_t(kSideAttached), std::memory_order_release);
			events_ |= kEvDetached;
			Log(kLogInfo, "detached (session %u)", session_);
		}
		CloseMapping(0);
	}
}
