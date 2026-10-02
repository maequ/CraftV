// A minimal, dependency-free test harness for redcraft_link_tests.
#pragma once

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace redcraft::test
{
	struct Case
	{
		const char*           name;
		std::function<void()> fn;
	};

	std::vector<Case>& Registry();
	void               Fail(const char* a_file, int a_line, const std::string& a_what);
	bool               CurrentFailed();

	struct Registrar
	{
		Registrar(const char* a_name, std::function<void()> a_fn) { Registry().push_back({ a_name, std::move(a_fn) }); }
	};

	std::string Hex(const void* a_data, std::size_t a_bytes);
}

#define REDCRAFT_CAT2(a, b) a##b
#define REDCRAFT_CAT(a, b) REDCRAFT_CAT2(a, b)
#define TEST_CASE(name)                                                                                       \
	static void REDCRAFT_CAT(test_fn_, __LINE__)();                                                             \
	static ::redcraft::test::Registrar REDCRAFT_CAT(test_reg_, __LINE__)(name, &REDCRAFT_CAT(test_fn_, __LINE__)); \
	static void REDCRAFT_CAT(test_fn_, __LINE__)()

#define CHECK(cond)                                                     \
	do {                                                                \
		if (!(cond)) ::redcraft::test::Fail(__FILE__, __LINE__, #cond); \
	} while (0)

#define REQUIRE(cond)                                                   \
	do {                                                                \
		if (!(cond)) {                                                  \
			::redcraft::test::Fail(__FILE__, __LINE__, #cond);            \
			return;                                                     \
		}                                                               \
	} while (0)

#define CHECK_EQ(a, b)                                                                                                          \
	do {                                                                                                                        \
		const auto& va_ = (a);                                                                                                  \
		const auto& vb_ = (b);                                                                                                  \
		if (!(va_ == vb_))                                                                                                      \
			::redcraft::test::Fail(__FILE__, __LINE__, std::string(#a " == " #b " (") + std::to_string(va_) + " vs " + std::to_string(vb_) + ")"); \
	} while (0)
