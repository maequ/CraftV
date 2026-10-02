// The two clocks the protocol uses (PROTOCOL.md §1): GetTickCount64 for timeouts and
// QueryPerformanceCounter (as microseconds) for message timestamps. Same clocks as the Java side.
#pragma once

#include <cstdint>

namespace redcraft::clock
{
	std::uint64_t NowMs();  // GetTickCount64
	std::uint64_t NowUs();  // QueryPerformanceCounter in microseconds (system-wide)
}
