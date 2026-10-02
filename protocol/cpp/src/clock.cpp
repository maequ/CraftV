#include "redcraft/clock.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace redcraft::clock
{
	namespace
	{
		constexpr std::uint64_t kUsPerSecond = 1000000;

		std::uint64_t Frequency()
		{
			static const std::uint64_t frequency = [] {
				LARGE_INTEGER f{};
				::QueryPerformanceFrequency(&f);
				return static_cast<std::uint64_t>(f.QuadPart);
			}();
			return frequency;
		}
	}

	std::uint64_t NowMs()
	{
		return ::GetTickCount64();
	}

	std::uint64_t NowUs()
	{
		LARGE_INTEGER c{};
		::QueryPerformanceCounter(&c);
		const auto counter = static_cast<std::uint64_t>(c.QuadPart);
		const auto f = Frequency();
		// Split to avoid overflow: counter * 1e6 overflows u64 after ~21 days at 10 MHz.
		return (counter / f) * kUsPerSecond + (counter % f) * kUsPerSecond / f;
	}
}
