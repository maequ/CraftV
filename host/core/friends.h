// The friends Minecraft reports (PROTOCOL.md §7.8-7.10), as the host sees them: a fixed table, no heap.
// Phase 2 shows them on the overlay; Phase 3 spawns a character for each.
#pragma once

#include "craftv/protocol.h"

#include <cstdint>
#include <cstring>

namespace craftv::host
{
	struct Friend
	{
		bool                        used = false;
		std::uint32_t               id = 0;
		char                        name[proto::kPlayerNameMaxBytes + 1] = {};
		bool                        hasState = false;
		proto::RemotePlayerStateMsg last{};
		std::uint64_t               lastUpdateMs = 0;
	};

	class Friends
	{
	public:
		static constexpr int kMax = 16;

		// §7.8: a JOIN for a known id replaces it. False when the table is full.
		bool Join(const proto::RemotePlayerJoinMsg& a_msg)
		{
			Friend* f = Find(a_msg.playerId);
			if (!f) {
				f = FreeSlot();
				if (!f) {
					return false;
				}
			}
			*f = Friend{};
			f->used = true;
			f->id = a_msg.playerId;
			const std::size_t n = a_msg.nameBytes <= proto::kPlayerNameMaxBytes ? a_msg.nameBytes : proto::kPlayerNameMaxBytes;
			std::memcpy(f->name, a_msg.name, n);
			f->name[n] = '\0';
			return true;
		}

		// §7.10: a state for an unknown id is ignored (normal right after a restart).
		bool State(const proto::RemotePlayerStateMsg& a_msg, std::uint64_t a_nowMs)
		{
			Friend* f = Find(a_msg.playerId);
			if (!f) {
				return false;
			}
			f->last = a_msg;
			f->hasState = true;
			f->lastUpdateMs = a_nowMs;
			return true;
		}

		void Leave(std::uint32_t a_id)
		{
			if (Friend* f = Find(a_id)) {
				*f = Friend{};
			}
		}

		void Clear()
		{
			for (auto& f : slots_) {
				f = Friend{};
			}
		}

		int Count() const
		{
			int n = 0;
			for (const auto& f : slots_) {
				n += f.used ? 1 : 0;
			}
			return n;
		}

		const Friend& Slot(int a_i) const { return slots_[a_i]; }

	private:
		Friend* Find(std::uint32_t a_id)
		{
			for (auto& f : slots_) {
				if (f.used && f.id == a_id) {
					return &f;
				}
			}
			return nullptr;
		}

		Friend* FreeSlot()
		{
			for (auto& f : slots_) {
				if (!f.used) {
					return &f;
				}
			}
			return nullptr;
		}

		Friend slots_[kMax];
	};
}
