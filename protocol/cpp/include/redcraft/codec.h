// Record encoding and message validation (PROTOCOL.md §4.2, §7, §8). No heap allocation.
#pragma once

#include "redcraft/protocol.h"

#include <cstring>

namespace redcraft::codec
{
	using namespace redcraft::proto;

	// Writes one complete record (header + payload + zero padding) into a_out. Returns the record
	// size, or 0 if a_cap is too small or the payload is too large. (PROTOCOL.md §4.2)
	std::size_t EncodeRecord(std::uint8_t* a_out, std::size_t a_cap, std::uint16_t a_type, std::uint16_t a_typeVersion,
		std::uint32_t a_session, std::uint32_t a_seq, const void* a_payload, std::uint32_t a_payloadBytes);

	// The fixed payload size of a core message type, or 0 for PAD / unknown / variable types. (§7)
	std::uint32_t FixedPayloadBytes(std::uint16_t a_type);

	// May a side with this role send this message type? (§7 "Dir" column)
	bool AllowedFrom(std::uint16_t a_type, Role a_sender);

	// Range checks on decoded values (§7.x, §8). True = acceptable.
	bool Valid(const HelloMsg& a_msg);
	bool Valid(const HeartbeatMsg& a_msg);
	bool Valid(const PlayerStateMsg& a_msg);
	bool Valid(const BlockSetMsg& a_msg);
	bool Valid(const BlockBreakRequestMsg& a_msg);
	bool Valid(const BlockPlaceRequestMsg& a_msg);
	bool Valid(const LogMsg& a_msg);

	// Copies the known prefix of a payload into a_out and validates it. Shorter payloads are malformed;
	// longer ones are accepted (forward compatibility, §9). The payload pointer may point into shared
	// memory: it is read exactly once, by the memcpy.
	template <class T>
	bool Decode(const RecordHeader& a_header, const std::uint8_t* a_payload, T& a_out)
	{
		if (a_header.type != T::kType || a_header.payloadBytes < sizeof(T)) {
			return false;
		}
		std::memcpy(&a_out, a_payload, sizeof(T));
		return Valid(a_out);
	}

	// ---- builders --------------------------------------------------------------------------------
	HelloMsg MakeHello(Role a_role, std::uint32_t a_pid, std::uint32_t a_session, const char* a_software);
	LogMsg   MakeLog(LogLevel a_level, const char* a_text);

	// ---- TEST_PATTERN (§7.7) ---------------------------------------------------------------------
	std::uint32_t Fnv1a32(const std::uint8_t* a_data, std::size_t a_bytes);
	// Payload size the stress sender uses for this index.
	std::uint32_t TestPatternPayloadBytes(std::uint64_t a_index);
	// Builds the payload into a_out (capacity >= kTestPatternFixedBytes + kTestPatternMaxFill). Returns its size.
	std::uint32_t BuildTestPattern(std::uint64_t a_index, std::uint8_t* a_out);
	// Checks a received payload. On success writes the index.
	bool CheckTestPattern(const std::uint8_t* a_payload, std::uint32_t a_payloadBytes, std::uint64_t& a_index);
}
