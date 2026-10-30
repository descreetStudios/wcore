#pragma once

#include <functional>
#include <string>
#include <sstream>
#include <vector>

namespace wcore
{
	namespace test
	{
		struct Case
		{
			std::string name;
			std::function<void()> fn;
		};

		std::vector<Case>& registry();
		void fail(const char* file, int line, const std::string& msg);

		struct Abort {};

		struct Registrar
		{
			Registrar(const char* name, std::function<void()> fn)
			{
				registry().push_back({ name, std::move(fn) });
			}
		};

		template <class A, class B>
		void check_eq(const char* file, int line, const char* ea,
			const char* eb, const A& a, const B& b)
		{
			if (!(a == b)) // for older types
			{
				std::ostringstream os;
				os << ea << " == " << eb << " (" << a << " vs " << b << ")";
				fail(file, line, os.str());
			}
		}
	} // namespace test

#define TEST(name)														\
    static void name();													\
    static ::wcore::test::Registrar registrar_##name(#name, name);      \
    static void name()

// Non-fatal: records the failure and continues.
#define CHECK(expr) \
    do { if (!(expr)) ::wcore::test::fail(__FILE__, __LINE__, #expr); } while (0)

#define CHECK_EQ(a, b) ::wcore::test::check_eq(__FILE__, __LINE__, #a, #b, (a), (b))

// Fatal: records the failure and stops the current test.
#define REQUIRE(expr) \
    do { if (!(expr)) { ::wcore::test::fail(__FILE__, __LINE__, #expr); throw ::wcore::test::Abort{}; } } while (0)
}