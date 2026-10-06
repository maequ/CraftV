// Endpoint tests (PROTOCOL.md §5): two endpoints in one process on a private mapping name, driven
// by a fake clock, so connect / stale / resume / restart / clean detach are deterministic.
#include "test.h"

#include "craftv/endpoint.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

using namespace craftv;
using namespace craftv::proto;

namespace
{
	std::wstring UniqueName(const wchar_t* a_tag)
	{
		return std::wstring(L"Local\\CraftV_Test_") + a_tag + L"_" + std::to_wstring(::GetCurrentProcessId()) + L"_" +
		       std::to_wstring(::GetTickCount64());
	}

	EndpointConfig Config(Role a_role, const std::wstring& a_name)
	{
		EndpointConfig c;
		c.role = a_role;
		c.mappingName = a_name.c_str();
		c.software = a_role == Role::kHost ? "test-host" : "test-mc";
		c.peerTimeoutMs = 1000;
		return c;
	}

	struct Clock
	{
		std::uint64_t ms = 100000;
		std::uint64_t Us() const { return ms * 1000; }
	};

	void TickBoth(Endpoint& a, Endpoint& b, Clock& clock, int a_ticks, std::uint64_t a_stepMs = 16)
	{
		for (int i = 0; i < a_ticks; ++i) {
			clock.ms += a_stepMs;
			a.Tick(clock.ms, clock.Us());
			b.Tick(clock.ms, clock.Us());
			a.Drain(clock.Us(), kMaxDrainBytesPerTick, [](const RecordHeader&, const std::uint8_t*) {});
			b.Drain(clock.Us(), kMaxDrainBytesPerTick, [](const RecordHeader&, const std::uint8_t*) {});
		}
	}
}

TEST_CASE("endpoint: host and MC connect, exchange HELLO, and measure RTT")
{
	const auto name = UniqueName(L"ep");
	Clock      clock;
	Endpoint   host(Config(Role::kHost, name));
	Endpoint   mc(Config(Role::kMc, name));
	TickBoth(host, mc, clock, 5);
	CHECK(host.State() == LinkState::kConnected);
	CHECK(mc.State() == LinkState::kConnected);
	CHECK(host.WasCreator());
	CHECK(!mc.WasCreator());
	TickBoth(host, mc, clock, 60);  // > one HEARTBEAT_MSG period each way
	CHECK(host.Peer().helloSeen);
	CHECK(mc.Peer().helloSeen);
	CHECK_EQ(mc.Peer().hello.role, static_cast<std::uint32_t>(Role::kHost));
	CHECK(host.Peer().rttValid);
	CHECK((host.TakeEvents() & kEvConnected) != 0);
	CHECK_EQ(host.RxStats().malformed + host.RxStats().corrupt + mc.RxStats().malformed + mc.RxStats().corrupt, std::uint64_t(0));
}

TEST_CASE("endpoint: either side may start first")
{
	const auto name = UniqueName(L"order");
	Clock      clock;
	auto       mc = std::make_unique<Endpoint>(Config(Role::kMc, name));
	for (int i = 0; i < 10; ++i) {
		clock.ms += 16;
		mc->Tick(clock.ms, clock.Us());
	}
	CHECK(mc->State() == LinkState::kAttached);  // alone
	CHECK(mc->WasCreator());
	Endpoint host(Config(Role::kHost, name));
	TickBoth(host, *mc, clock, 5);
	CHECK(host.State() == LinkState::kConnected);
	CHECK(mc->State() == LinkState::kConnected);
}

TEST_CASE("endpoint: gameplay messages flow both ways and are refused before CONNECTED")
{
	const auto name = UniqueName(L"msgs");
	Clock      clock;
	Endpoint   host(Config(Role::kHost, name));
	PlayerStateMsg ps{};
	ps.y = 64;
	host.Tick(clock.ms, clock.Us());
	CHECK(!host.Send(ps));  // alone: not connected
	CHECK(host.DroppedNotConnected() > 0);

	Endpoint mc(Config(Role::kMc, name));
	TickBoth(host, mc, clock, 5);
	REQUIRE(host.Connected());
	ps.x = 3.5;
	CHECK(host.Send(ps));
	BlockSetMsg bs{ 1, 2, 3, 4, 0, 0 };
	CHECK(mc.Send(bs));
	int gotPs = 0, gotBs = 0;
	mc.Drain(clock.Us(), kMaxDrainBytesPerTick, [&](const RecordHeader& h, const std::uint8_t* p) {
		PlayerStateMsg m;
		if (h.type == kMsgPlayerState && codec::Decode(h, p, m) && m.x == 3.5) {
			++gotPs;
		}
	});
	host.Drain(clock.Us(), kMaxDrainBytesPerTick, [&](const RecordHeader& h, const std::uint8_t* p) {
		BlockSetMsg m;
		if (h.type == kMsgBlockSet && codec::Decode(h, p, m) && m.z == 3) {
			++gotBs;
		}
	});
	CHECK_EQ(gotPs, 1);
	CHECK_EQ(gotBs, 1);

	// PLAYER_STATE from MC is reserved (PROTOCOL.md §7.3): the host counts it as malformed.
	CHECK(mc.Send(ps));
	host.Drain(clock.Us(), kMaxDrainBytesPerTick, [&](const RecordHeader&, const std::uint8_t*) { CHECK(false); });
	CHECK_EQ(host.RxStats().malformed, std::uint64_t(1));
}

TEST_CASE("endpoint: every core message type reaches the other side (none is dropped as unknown)")
{
	const auto name = UniqueName(L"alltypes");
	Clock      clock;
	Endpoint   host(Config(Role::kHost, name));
	Endpoint   mc(Config(Role::kMc, name));
	TickBoth(host, mc, clock, 5);
	REQUIRE(host.Connected());
	std::vector<std::uint16_t> fromHost, fromMc, toMc, toHost;
	std::uint8_t               payload[kMaxPayload] = {};
	for (std::uint16_t type = kMsgPlayerState; type <= 0xFF; ++type) {
		const std::uint32_t bytes = codec::FixedPayloadBytes(type);
		if (bytes == 0) {
			continue;  // not a core fixed-size type
		}
		if (codec::AllowedFrom(type, Role::kHost) && host.Send(type, payload, bytes)) fromHost.push_back(type);
		if (codec::AllowedFrom(type, Role::kMc) && mc.Send(type, payload, bytes)) fromMc.push_back(type);
	}
	mc.Drain(clock.Us(), kMaxDrainBytesPerTick, [&](const RecordHeader& h, const std::uint8_t*) { toMc.push_back(h.type); });
	host.Drain(clock.Us(), kMaxDrainBytesPerTick, [&](const RecordHeader& h, const std::uint8_t*) { toHost.push_back(h.type); });
	CHECK(fromHost == toMc);
	CHECK(fromMc == toHost);
	CHECK(std::find(toMc.begin(), toMc.end(), std::uint16_t(kMsgCamera)) != toMc.end());
	CHECK(std::find(toHost.begin(), toHost.end(), std::uint16_t(kMsgOwnerState)) != toHost.end());
	CHECK_EQ(host.RxStats().unknown + mc.RxStats().unknown, std::uint64_t(0));
}

TEST_CASE("endpoint: a frozen heartbeat goes STALE, and resumes with the same session")
{
	const auto name = UniqueName(L"stale");
	Clock      clock;
	Endpoint   host(Config(Role::kHost, name));
	Endpoint   mc(Config(Role::kMc, name));
	TickBoth(host, mc, clock, 5);
	REQUIRE(mc.Connected());
	host.TakeEvents();
	mc.TakeEvents();
	const auto hostSession = host.Session();

	host.SetHeartbeatSuspended(true);  // "kill-link"
	TickBoth(host, mc, clock, 80);     // 1.28 s > 1 s timeout
	CHECK(mc.State() == LinkState::kStale);
	CHECK((mc.TakeEvents() & kEvStale) != 0);
	CHECK(host.State() == LinkState::kConnected);  // MC still beats

	host.SetHeartbeatSuspended(false);
	TickBoth(host, mc, clock, 3);
	CHECK(mc.State() == LinkState::kConnected);
	const auto ev = mc.TakeEvents();
	CHECK((ev & kEvResumed) != 0);
	CHECK((ev & kEvPeerRestarted) == 0);
	CHECK_EQ(host.Session(), hostSession);
}

TEST_CASE("endpoint: a restarted host is a new session; MC drops its stale records")
{
	const auto name = UniqueName(L"restart");
	Clock      clock;
	auto       host = std::make_unique<Endpoint>(Config(Role::kHost, name));
	Endpoint   mc(Config(Role::kMc, name));
	TickBoth(*host, mc, clock, 5);
	REQUIRE(mc.Connected());
	const auto oldSession = host->Session();

	// The old host queues messages MC never reads, then dies without a clean detach.
	PlayerStateMsg ps{};
	for (int i = 0; i < 5; ++i) {
		host->Send(ps);
	}
	host->SetHeartbeatSuspended(true);
	host.reset();  // clean detach; its unread records stay in the ring

	auto host2 = std::make_unique<Endpoint>(Config(Role::kHost, name));
	int  received = 0;
	for (int i = 0; i < 10; ++i) {
		clock.ms += 16;
		host2->Tick(clock.ms, clock.Us());
		mc.Tick(clock.ms, clock.Us());
		mc.Drain(clock.Us(), kMaxDrainBytesPerTick, [&](const RecordHeader& h, const std::uint8_t*) {
			if (h.type == kMsgPlayerState) {
				++received;
			}
		});
		host2->Drain(clock.Us(), kMaxDrainBytesPerTick, [](const RecordHeader&, const std::uint8_t*) {});
	}
	CHECK(host2->Session() == oldSession + 1);
	CHECK(mc.State() == LinkState::kConnected);
	CHECK((mc.TakeEvents() & kEvPeerRestarted) != 0);
	CHECK_EQ(received, 0);                         // the 5 old PLAYER_STATEs were stale
	CHECK_EQ(mc.RxStats().stale, std::uint64_t(5));
}

TEST_CASE("endpoint: a restarted MC discards the old backlog and the host sees PeerRestarted")
{
	const auto name = UniqueName(L"mcrestart");
	Clock      clock;
	Endpoint   host(Config(Role::kHost, name));
	auto       mc = std::make_unique<Endpoint>(Config(Role::kMc, name));
	TickBoth(host, *mc, clock, 5);
	REQUIRE(host.Connected());
	host.TakeEvents();
	PlayerStateMsg ps{};
	for (int i = 0; i < 7; ++i) {
		host.Send(ps);  // queued for the old MC, never read
	}
	mc.reset();
	clock.ms += 16;
	host.Tick(clock.ms, clock.Us());
	CHECK(host.State() == LinkState::kAttached);  // clean detach seen at once
	CHECK((host.TakeEvents() & kEvPeerDetached) != 0);

	auto mc2 = std::make_unique<Endpoint>(Config(Role::kMc, name));
	int  received = 0;
	for (int i = 0; i < 5; ++i) {
		clock.ms += 16;
		host.Tick(clock.ms, clock.Us());
		mc2->Tick(clock.ms, clock.Us());
		mc2->Drain(clock.Us(), kMaxDrainBytesPerTick, [&](const RecordHeader& h, const std::uint8_t*) {
			if (h.type == kMsgPlayerState) {
				++received;
			}
		});
	}
	CHECK(host.State() == LinkState::kConnected);
	CHECK_EQ(received, 0);  // backlog discarded on attach
	CHECK(mc2->Peer().helloSeen);  // host re-sent HELLO for the new session
}

TEST_CASE("endpoint: a crashed peer that left ATTACHED set goes STALE, not CONNECTED")
{
	const auto name = UniqueName(L"crash");
	Clock      clock;
	auto       mc = std::make_unique<Endpoint>(Config(Role::kMc, name));
	Endpoint   keeper(Config(Role::kHost, name));  // keeps the mapping alive
	TickBoth(keeper, *mc, clock, 5);
	keeper.Detach();
	// Re-open a host later while MC "crashes": freeze MC's beat but leave its ATTACHED bit set.
	mc->SetHeartbeatSuspended(true);
	Endpoint host(Config(Role::kHost, name));
	for (int i = 0; i < 100; ++i) {
		clock.ms += 16;
		host.Tick(clock.ms, clock.Us());
		mc->Tick(clock.ms, clock.Us());
	}
	CHECK(host.State() == LinkState::kStale);
}

TEST_CASE("endpoint: a second endpoint of the same role refuses to attach while the first lives")
{
	const auto name = UniqueName(L"dup");
	Clock      clock;
	Endpoint   host1(Config(Role::kHost, name));
	host1.Tick(clock.ms, clock.Us());
	REQUIRE(host1.State() == LinkState::kAttached);
	// Same process, so the pid check can't tell them apart: make host1 look like another live
	// process (pid 4, the System process, always exists).
	SharedMapping peek;
	char          err[160];
	REQUIRE(peek.CreateOrOpen(name.c_str(), Role::kMc, err, sizeof(err)));
	REQUIRE(peek.PollReady(err, sizeof(err)) == ReadyResult::kReady);
	peek.HeaderPtr()->host.pid = 4;
	Endpoint host2(Config(Role::kHost, name));
	host2.Tick(clock.ms, clock.Us());
	CHECK(host2.State() == LinkState::kDetached);
}
