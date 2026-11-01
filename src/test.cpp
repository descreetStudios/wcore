#include <wcore/test.hpp>

#include <exception>
#include <iostream>

namespace wcore::test
{
	static int gFailuresInTest{ 0 };

	std::vector<Case>& registry()
	{
		static std::vector<Case> cases;
		return cases;
	}

	void fail(const char* file, int line, const std::string& msg)
	{
		++gFailuresInTest;
		std::cerr << " " << file << ":" << line << ": FAILED: " << msg << "\n";
	}
}

int main(int argc, char** argv)
{
	const char* filter{ argc > 1 ? argv[1] : nullptr };

	int run{ 0 }, failed{ 0 };
	for (auto& c : wcore::test::registry())
	{
		if (filter && c.name.find(filter) == std::string::npos) continue;

		++run;
		wcore::test::gFailuresInTest = 0;
		std::cout << "[ RUN ] " << c.name << "\n";

		try {
			c.fn();
		}
		catch (const wcore::test::Abort&) {
			// Already reported by REQUIRE.
		}
		catch (const std::exception& e) {
			wcore::test::fail(__FILE__, __LINE__,
				std::string("unhandled exception: ") + e.what());
		}
		catch (...) {
			wcore::test::fail(__FILE__, __LINE__, "unhandled unknown exception");
		}

		if (wcore::test::gFailuresInTest) 
		{
			++failed;
			std::cout << "[ FAIL ] " << c.name << "\n";
		}
		else std::cout <<"[ OK ] " << c.name << "\n";
	}

	std::cout << "\n" << (run - failed) << "/" << run << " tests passed\n";
	return failed == 0 ? 0 : 1; // On POSIX systems the exit status is truncated to 8 bits.
}