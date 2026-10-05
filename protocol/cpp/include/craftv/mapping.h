// The named file mapping: create-or-open, creator initialisation, layout validation
// (PROTOCOL.md §2, §3, §5.1).
#pragma once

#include "craftv/protocol.h"

#include <cstddef>
#include <cstdint>

namespace craftv
{
	// Where one ring lives inside a validated mapping.
	struct RingLocation
	{
		proto::RingControl* control = nullptr;
		std::uint8_t*       data = nullptr;
		std::uint64_t       dataBytes = 0;
	};

	struct Layout
	{
		RingLocation hostToMc;
		RingLocation mcToHost;
	};

	// Writes a fresh v1.0 header, section table and ring control pages, then the magic (release).
	// Used by the creator and by init-timeout recovery (§5.1). a_base must span kMappingBytes.
	void InitializeMapping(std::uint8_t* a_base, proto::Role a_creatorRole, std::uint32_t a_creatorPid);

	// Validates everything a reader must check before attaching (§3.2). Pure function so tests can
	// feed it hand-made or corrupted buffers. a_viewBytes is the real size of the view.
	// On failure writes a reason into a_err.
	bool ValidateLayout(std::uint8_t* a_base, std::uint64_t a_viewBytes, Layout& a_out, char* a_err, std::size_t a_errCap);

	enum class ReadyResult
	{
		kReady,
		kNotYet,   // magic still zero: the creator is still initialising
		kInvalid,  // initialised but unusable (version, sizes, ...): see the error text
	};

	// Owns the Win32 mapping handle and view. Not thread-safe; one owner.
	class SharedMapping
	{
	public:
		SharedMapping() = default;
		SharedMapping(const SharedMapping&) = delete;
		SharedMapping& operator=(const SharedMapping&) = delete;
		~SharedMapping() { Close(); }

		// CreateFileMappingW + MapViewOfFile (§5.1). If this call created the mapping, it is
		// initialised immediately. Returns false (and an error text) on any Win32 failure.
		bool CreateOrOpen(const wchar_t* a_name, proto::Role a_myRole, char* a_err, std::size_t a_errCap);

		// Non-blocking: checks the magic and validates the layout.
		ReadyResult PollReady(char* a_err, std::size_t a_errCap);

		// Init-timeout recovery (§5.1): initialise a mapping whose creator never wrote the magic.
		void ForceInitialize(proto::Role a_myRole);

		void Close();

		bool                 IsOpen() const { return base_ != nullptr; }
		bool                 WasCreator() const { return creator_; }
		std::uint8_t*        Base() const { return base_; }
		std::uint64_t        ViewBytes() const { return viewBytes_; }
		proto::Header*       HeaderPtr() const { return reinterpret_cast<proto::Header*>(base_); }
		const Layout&        GetLayout() const { return layout_; }

	private:
		void*         handle_ = nullptr;
		std::uint8_t* base_ = nullptr;
		std::uint64_t viewBytes_ = 0;
		bool          creator_ = false;
		Layout        layout_{};
	};
}
