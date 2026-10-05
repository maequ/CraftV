#include "test.h"

#include <chrono>
#include <cstring>

namespace craftv::test
{
	namespace
	{
		bool g_failed = false;
		int  g_failures = 0;
	}

	std::vector<Case>& Registry()
	{
		static std::vector<Case> cases;
		return cases;
	}

	void Fail(const char* a_file, int a_line, const std::string& a_what)
	{
		g_failed = true;
		++g_failures;
		std::printf("    FAIL %s:%d: %s\n", a_file, a_line, a_what.c_str());
	}

	bool CurrentFailed() { return g_failed; }

	std::string Hex(const void* a_data, std::size_t a_bytes)
	{
		static const char digits[] = "0123456789abcdef";
		std::string       out;
		out.reserve(a_bytes * 2);
		const auto* p = static_cast<const unsigned char*>(a_data);
		for (std::size_t i = 0; i < a_bytes; ++i) {
			out.push_back(digits[p[i] >> 4]);
			out.push_back(digits[p[i] & 15]);
		}
		return out;
	}
}

int main(int argc, char** argv)
{
	using namespace craftv::test;
	const char* filter = argc > 1 ? argv[1] : nullptr;
	int         passed = 0, failed = 0;
	for (const auto& c : Registry()) {
		if (filter && !std::strstr(c.name, filter)) {
			continue;
		}
		g_failed = false;
		const auto start = std::chrono::steady_clock::now();
		std::printf("[ RUN  ] %s\n", c.name);
		std::fflush(stdout);
		c.fn();
		const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
		if (g_failed) {
			++failed;
			std::printf("[ FAIL ] %s (%lld ms)\n", c.name, static_cast<long long>(ms));
		} else {
			++passed;
			std::printf("[  OK  ] %s (%lld ms)\n", c.name, static_cast<long long>(ms));
		}
		std::fflush(stdout);
	}
	std::printf("\n%d passed, %d failed\n", passed, failed);
	return failed == 0 ? 0 : 1;
}
