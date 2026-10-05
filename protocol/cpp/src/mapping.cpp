#include "craftv/mapping.h"

#include "craftv/ring.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <sddl.h>

#include <cstdio>
#include <cstring>

namespace craftv
{
	using namespace craftv::proto;

	namespace
	{
		void SetError(char* a_err, std::size_t a_cap, const char* a_fmt, auto... a_args)
		{
			if (a_err && a_cap) {
				std::snprintf(a_err, a_cap, a_fmt, a_args...);
			}
		}

		bool IsPowerOfTwo(std::uint64_t a_value) { return a_value && (a_value & (a_value - 1)) == 0; }

		// Who may open the mapping: SYSTEM, Administrators and the current user, at medium
		// integrity. An elevated creator would otherwise make it admin-only and a normal Minecraft
		// couldn't open it (error 5). Same rule as SkyCraft's Link.cpp. Free with LocalFree.
		PSECURITY_DESCRIPTOR SharedWithThisUser()
		{
			wchar_t sddl[512] = L"D:P(A;;GA;;;SY)(A;;GA;;;BA)";
			HANDLE  token = nullptr;
			if (::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token)) {
				alignas(TOKEN_USER) std::uint8_t buffer[256];
				DWORD                            size = 0;
				if (::GetTokenInformation(token, TokenUser, buffer, sizeof(buffer), &size)) {
					LPWSTR sid = nullptr;
					if (::ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(buffer)->User.Sid, &sid)) {
						wcscat_s(sddl, L"(A;;GA;;;");
						wcscat_s(sddl, sid);
						wcscat_s(sddl, L")");
						::LocalFree(sid);
					}
				}
				::CloseHandle(token);
			}
			wcscat_s(sddl, L"S:(ML;;NW;;;ME)");
			PSECURITY_DESCRIPTOR descriptor = nullptr;
			if (!::ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, SDDL_REVISION_1, &descriptor, nullptr)) {
				return nullptr;  // fall back to the default DACL
			}
			return descriptor;
		}

		const SectionEntry* FindSection(const Header& a_header, std::uint32_t a_id)
		{
			for (std::uint32_t i = 0; i < a_header.sectionCount && i < kMaxSections; ++i) {
				if (a_header.sections[i].id == a_id) {
					return &a_header.sections[i];
				}
			}
			return nullptr;
		}

		bool ValidateRing(std::uint8_t* a_base, const Header& a_header, std::uint32_t a_id, Role a_producer, RingLocation& a_out,
			char* a_err, std::size_t a_cap)
		{
			const SectionEntry* s = FindSection(a_header, a_id);
			if (!s) {
				SetError(a_err, a_cap, "ring section %u missing", a_id);
				return false;
			}
			if (s->offset % kSectionAlign != 0 || s->offset < a_header.headerBytes || s->bytes <= kRingControlBytes ||
				s->offset > a_header.mappingBytes || s->bytes > a_header.mappingBytes - s->offset) {
				SetError(a_err, a_cap, "ring section %u out of bounds (offset 0x%llx, bytes 0x%llx)", a_id,
					static_cast<unsigned long long>(s->offset), static_cast<unsigned long long>(s->bytes));
				return false;
			}
			const std::uint64_t dataBytes = s->bytes - kRingControlBytes;
			if (!IsPowerOfTwo(dataBytes) || dataBytes < kMinRingDataBytes || dataBytes > kMaxRingDataBytes) {
				SetError(a_err, a_cap, "ring section %u data size 0x%llx not a power of two in range", a_id,
					static_cast<unsigned long long>(dataBytes));
				return false;
			}
			auto* control = reinterpret_cast<RingControl*>(a_base + s->offset);
			if (control->dataBytes != dataBytes || control->producerRole != static_cast<std::uint32_t>(a_producer)) {
				SetError(a_err, a_cap, "ring section %u control page disagrees with the section table", a_id);
				return false;
			}
			a_out.control = control;
			a_out.data = a_base + s->offset + kRingControlBytes;
			a_out.dataBytes = dataBytes;
			return true;
		}
	}

	void InitializeMapping(std::uint8_t* a_base, Role a_creatorRole, std::uint32_t a_creatorPid)
	{
		auto* header = reinterpret_cast<Header*>(a_base);
		// Everything except the magic, which goes last (§5.1). The side blocks are left alone: a
		// recovering side may find a peer's session there that it must not lose.
		header->versionMajor = kVersionMajor;
		header->versionMinor = kVersionMinor;
		header->headerBytes = static_cast<std::uint32_t>(kHeaderBytes);
		header->sectionCount = kSectionCount;
		header->mappingBytes = kMappingBytes;
		header->creatorRole = static_cast<std::uint32_t>(a_creatorRole);
		header->creatorPid = a_creatorPid;
		std::memset(header->sections, 0, sizeof(header->sections));
		header->sections[0] = { kSectionRingHostToMc, 0, kOffRingHostToMc, kRingSectionBytes, 0 };
		header->sections[1] = { kSectionRingMcToHost, 0, kOffRingMcToHost, kRingSectionBytes, 0 };
		const struct
		{
			std::uint64_t offset;
			Role          producer;
		} rings[] = { { kOffRingHostToMc, Role::kHost }, { kOffRingMcToHost, Role::kMc } };
		for (const auto& r : rings) {
			auto* control = reinterpret_cast<RingControl*>(a_base + r.offset);
			std::memset(control, 0, sizeof(RingControl));
			control->dataBytes = kRingDataBytes;
			control->producerRole = static_cast<std::uint32_t>(r.producer);
		}
		detail::Atomic(header->magic).store(kMagic, std::memory_order_release);
	}

	bool ValidateLayout(std::uint8_t* a_base, std::uint64_t a_viewBytes, Layout& a_out, char* a_err, std::size_t a_errCap)
	{
		if (a_viewBytes < kHeaderBytes) {
			SetError(a_err, a_errCap, "view too small (%llu bytes)", static_cast<unsigned long long>(a_viewBytes));
			return false;
		}
		auto* header = reinterpret_cast<Header*>(a_base);
		const auto magic = detail::Atomic(header->magic).load(std::memory_order_acquire);
		if (magic != kMagic) {
			SetError(a_err, a_errCap, "bad magic 0x%08x", magic);
			return false;
		}
		if (header->versionMajor != kVersionMajor) {
			SetError(a_err, a_errCap, "protocol major version %u, expected %u", header->versionMajor, kVersionMajor);
			return false;
		}
		if (header->headerBytes != kHeaderBytes) {
			SetError(a_err, a_errCap, "headerBytes 0x%x, expected 0x%llx", header->headerBytes, static_cast<unsigned long long>(kHeaderBytes));
			return false;
		}
		if (header->sectionCount < 1 || header->sectionCount > kMaxSections) {
			SetError(a_err, a_errCap, "sectionCount %u out of range", header->sectionCount);
			return false;
		}
		if (header->mappingBytes > a_viewBytes || header->mappingBytes < kHeaderBytes) {
			SetError(a_err, a_errCap, "mappingBytes 0x%llx exceeds the view (0x%llx)", static_cast<unsigned long long>(header->mappingBytes),
				static_cast<unsigned long long>(a_viewBytes));
			return false;
		}
		Layout layout{};
		if (!ValidateRing(a_base, *header, kSectionRingHostToMc, Role::kHost, layout.hostToMc, a_err, a_errCap) ||
			!ValidateRing(a_base, *header, kSectionRingMcToHost, Role::kMc, layout.mcToHost, a_err, a_errCap)) {
			return false;
		}
		const auto aStart = reinterpret_cast<std::uintptr_t>(layout.hostToMc.control);
		const auto aEnd = reinterpret_cast<std::uintptr_t>(layout.hostToMc.data) + layout.hostToMc.dataBytes;
		const auto bStart = reinterpret_cast<std::uintptr_t>(layout.mcToHost.control);
		const auto bEnd = reinterpret_cast<std::uintptr_t>(layout.mcToHost.data) + layout.mcToHost.dataBytes;
		if (aStart < bEnd && bStart < aEnd) {
			SetError(a_err, a_errCap, "ring sections overlap");
			return false;
		}
		a_out = layout;
		return true;
	}

	bool SharedMapping::CreateOrOpen(const wchar_t* a_name, Role a_myRole, char* a_err, std::size_t a_errCap)
	{
		Close();
		SECURITY_ATTRIBUTES access{ sizeof(access), SharedWithThisUser(), FALSE };
		HANDLE              handle = ::CreateFileMappingW(INVALID_HANDLE_VALUE, access.lpSecurityDescriptor ? &access : nullptr, PAGE_READWRITE,
						 static_cast<DWORD>(kMappingBytes >> 32), static_cast<DWORD>(kMappingBytes & 0xFFFFFFFF), a_name);
		const DWORD         createError = ::GetLastError();
		if (access.lpSecurityDescriptor) {
			::LocalFree(access.lpSecurityDescriptor);
		}
		if (!handle) {
			SetError(a_err, a_errCap, "CreateFileMappingW failed (Windows error %lu%s)", createError,
				createError == ERROR_ACCESS_DENIED ? ": access denied; is the other side running as a different user?" : "");
			return false;
		}
		auto* view = static_cast<std::uint8_t*>(::MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, 0));
		if (!view) {
			SetError(a_err, a_errCap, "MapViewOfFile failed (Windows error %lu)", ::GetLastError());
			::CloseHandle(handle);
			return false;
		}
		// The real size of what we mapped. An existing mapping may be smaller than we asked for.
		MEMORY_BASIC_INFORMATION info{};
		if (::VirtualQuery(view, &info, sizeof(info)) == 0) {
			SetError(a_err, a_errCap, "VirtualQuery failed (Windows error %lu)", ::GetLastError());
			::UnmapViewOfFile(view);
			::CloseHandle(handle);
			return false;
		}
		handle_ = handle;
		base_ = view;
		viewBytes_ = info.RegionSize;
		creator_ = createError != ERROR_ALREADY_EXISTS;
		if (creator_) {
			if (viewBytes_ < kMappingBytes) {
				SetError(a_err, a_errCap, "new mapping smaller than requested (0x%llx)", static_cast<unsigned long long>(viewBytes_));
				Close();
				return false;
			}
			InitializeMapping(base_, a_myRole, ::GetCurrentProcessId());
		}
		return true;
	}

	ReadyResult SharedMapping::PollReady(char* a_err, std::size_t a_errCap)
	{
		if (!base_) {
			SetError(a_err, a_errCap, "not open");
			return ReadyResult::kInvalid;
		}
		const auto magic = detail::Atomic(HeaderPtr()->magic).load(std::memory_order_acquire);
		if (magic == 0) {
			return ReadyResult::kNotYet;
		}
		return ValidateLayout(base_, viewBytes_, layout_, a_err, a_errCap) ? ReadyResult::kReady : ReadyResult::kInvalid;
	}

	void SharedMapping::ForceInitialize(Role a_myRole)
	{
		if (base_ && viewBytes_ >= kMappingBytes) {
			InitializeMapping(base_, a_myRole, ::GetCurrentProcessId());
		}
	}

	void SharedMapping::Close()
	{
		if (base_) {
			::UnmapViewOfFile(base_);
			base_ = nullptr;
		}
		if (handle_) {
			::CloseHandle(static_cast<HANDLE>(handle_));
			handle_ = nullptr;
		}
		viewBytes_ = 0;
		creator_ = false;
		layout_ = {};
	}
}
