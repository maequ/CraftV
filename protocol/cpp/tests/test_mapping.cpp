// Mapping tests (PROTOCOL.md §3.2, §5.1): creator initialisation, a second opener in the same
// process, and that ValidateLayout refuses every kind of bad header.
#include "test.h"

#include "craftv/mapping.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <cstring>
#include <memory>
#include <string>

using namespace craftv;
using namespace craftv::proto;

namespace
{
	std::wstring UniqueName(const wchar_t* a_tag)
	{
		return std::wstring(L"Local\\CraftV_Test_") + a_tag + L"_" + std::to_wstring(::GetCurrentProcessId()) + L"_" +
		       std::to_wstring(::GetTickCount64());
	}

	struct Buffer
	{
		std::unique_ptr<std::uint8_t[]> bytes{ new std::uint8_t[kMappingBytes] };
		Buffer()
		{
			std::memset(bytes.get(), 0, kMappingBytes);
			InitializeMapping(bytes.get(), Role::kHost, 1);
		}
		Header* H() { return reinterpret_cast<Header*>(bytes.get()); }
		bool    Valid(std::uint64_t a_view = kMappingBytes)
		{
			Layout layout;
			char   err[160];
			return ValidateLayout(bytes.get(), a_view, layout, err, sizeof(err));
		}
	};
}

TEST_CASE("mapping: a fresh layout validates and points at the documented offsets")
{
	Buffer b;
	Layout layout;
	char   err[160] = {};
	REQUIRE(ValidateLayout(b.bytes.get(), kMappingBytes, layout, err, sizeof(err)));
	CHECK(reinterpret_cast<std::uint8_t*>(layout.hostToMc.control) == b.bytes.get() + kOffRingHostToMc);
	CHECK(layout.hostToMc.data == b.bytes.get() + kOffRingHostToMc + kRingControlBytes);
	CHECK(reinterpret_cast<std::uint8_t*>(layout.mcToHost.control) == b.bytes.get() + kOffRingMcToHost);
	CHECK_EQ(layout.hostToMc.dataBytes, kRingDataBytes);
	CHECK_EQ(b.H()->magic, kMagic);
	const std::uint8_t magicBytes[4] = { 0x43, 0x52, 0x46, 0x56 };  // "CRFV"
	CHECK(std::memcmp(b.bytes.get(), magicBytes, 4) == 0);
}

TEST_CASE("mapping: ValidateLayout refuses bad headers")
{
	{ Buffer b; b.H()->magic = 0; CHECK(!b.Valid()); }
	{ Buffer b; b.H()->versionMajor = 2; CHECK(!b.Valid()); }
	{ Buffer b; b.H()->headerBytes = 0x800; CHECK(!b.Valid()); }
	{ Buffer b; b.H()->sectionCount = 0; CHECK(!b.Valid()); }
	{ Buffer b; b.H()->sectionCount = 99; CHECK(!b.Valid()); }
	{ Buffer b; CHECK(!b.Valid(kMappingBytes - 0x1000)); }  // view smaller than mappingBytes
	{ Buffer b; b.H()->sections[0].offset = 0x1001; CHECK(!b.Valid()); }  // misaligned
	{ Buffer b; b.H()->sections[0].offset = 0; CHECK(!b.Valid()); }  // inside the header
	{ Buffer b; b.H()->sections[1].bytes = kRingControlBytes + 3 * 65536; CHECK(!b.Valid()); }  // not a power of two
	{ Buffer b; b.H()->sections[1].bytes = 0xFFFFFFFFFFFF0000ull; CHECK(!b.Valid()); }  // overflow attempt
	{ Buffer b; b.H()->sections[1].id = 77; CHECK(!b.Valid()); }  // ring 2 missing
	{ Buffer b; b.H()->sections[1].offset = kOffRingHostToMc; CHECK(!b.Valid()); }  // rings overlap (and producer role mismatch)
	{ Buffer b; reinterpret_cast<RingControl*>(b.bytes.get() + kOffRingHostToMc)->dataBytes = 4096; CHECK(!b.Valid()); }
	{ Buffer b; reinterpret_cast<RingControl*>(b.bytes.get() + kOffRingMcToHost)->producerRole = 1; CHECK(!b.Valid()); }
}

TEST_CASE("mapping: first CreateOrOpen creates and initialises, the second opens the same memory")
{
	const auto    name = UniqueName(L"map");
	SharedMapping a, b;
	char          err[160] = {};
	REQUIRE(a.CreateOrOpen(name.c_str(), Role::kMc, err, sizeof(err)));
	CHECK(a.WasCreator());
	CHECK(a.ViewBytes() >= kMappingBytes);
	CHECK(a.PollReady(err, sizeof(err)) == ReadyResult::kReady);
	CHECK_EQ(a.HeaderPtr()->creatorRole, static_cast<std::uint32_t>(Role::kMc));
	REQUIRE(b.CreateOrOpen(name.c_str(), Role::kHost, err, sizeof(err)));
	CHECK(!b.WasCreator());
	CHECK(b.PollReady(err, sizeof(err)) == ReadyResult::kReady);
	a.HeaderPtr()->host.pid = 12345;
	CHECK_EQ(b.HeaderPtr()->host.pid, std::uint32_t(12345));
	a.Close();
	b.Close();
	// Both closed: the mapping is gone, so the next CreateOrOpen creates it again.
	SharedMapping c;
	REQUIRE(c.CreateOrOpen(name.c_str(), Role::kHost, err, sizeof(err)));
	CHECK(c.WasCreator());
	CHECK_EQ(c.HeaderPtr()->host.pid, std::uint32_t(0));
}

TEST_CASE("mapping: a mapping whose creator never wrote the magic reads as not-yet, then can be force-initialised")
{
	const auto name = UniqueName(L"noinit");
	// Simulate a creator that died mid-initialisation: a raw mapping with no header.
	HANDLE raw = ::CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, static_cast<DWORD>(kMappingBytes), name.c_str());
	REQUIRE(raw != nullptr);
	SharedMapping m;
	char          err[160] = {};
	REQUIRE(m.CreateOrOpen(name.c_str(), Role::kMc, err, sizeof(err)));
	CHECK(!m.WasCreator());
	CHECK(m.PollReady(err, sizeof(err)) == ReadyResult::kNotYet);
	m.ForceInitialize(Role::kMc);
	CHECK(m.PollReady(err, sizeof(err)) == ReadyResult::kReady);
	m.Close();
	::CloseHandle(raw);
}

TEST_CASE("mapping: an existing mapping smaller than the layout is refused, not overrun")
{
	const auto name = UniqueName(L"small");
	HANDLE     raw = ::CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, 0x2000, name.c_str());
	REQUIRE(raw != nullptr);
	auto* view = static_cast<std::uint8_t*>(::MapViewOfFile(raw, FILE_MAP_ALL_ACCESS, 0, 0, 0));
	REQUIRE(view != nullptr);
	// A hostile/old peer wrote a plausible header claiming the full size.
	auto* h = reinterpret_cast<Header*>(view);
	h->versionMajor = kVersionMajor;
	h->headerBytes = static_cast<std::uint32_t>(kHeaderBytes);
	h->sectionCount = 2;
	h->mappingBytes = kMappingBytes;
	h->magic = kMagic;
	SharedMapping m;
	char          err[160] = {};
	REQUIRE(m.CreateOrOpen(name.c_str(), Role::kMc, err, sizeof(err)));
	CHECK(m.ViewBytes() < kMappingBytes);
	CHECK(m.PollReady(err, sizeof(err)) == ReadyResult::kInvalid);
	m.Close();
	::UnmapViewOfFile(view);
	::CloseHandle(raw);
}
