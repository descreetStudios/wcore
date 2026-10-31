#include <wcore/test.hpp>
#include <wcore/io.hpp>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <initializer_list>
#include <span>
#include <string>
#include <system_error>
#include <vector>

using namespace wcore;
using namespace test;

namespace fs = std::filesystem;

namespace
{
	struct TempDir
	{
		fs::path path;

		TempDir()
		{
			std::error_code ec;
			path = fs::temp_directory_path(ec) / "wcore_io_tests";
			fs::remove_all(path, ec);
			fs::create_directories(path, ec);
		}

		~TempDir()
		{
			std::error_code ec;
			fs::remove_all(path, ec);
		}

		TempDir(const TempDir&) = delete;
		TempDir& operator=(const TempDir&) = delete;

		fs::path operator/(const char* name) const { return path / name; }
	};

	std::vector<std::byte> make_bytes(std::initializer_list<unsigned> values)
	{
		std::vector<std::byte> result;
		result.reserve(values.size());

		for (const unsigned value : values)
			result.push_back(static_cast<std::byte>(value));

		return result;
	}

	bool contains(const std::vector<fs::path>& entries, const fs::path& entry)
	{
		return std::find(entries.begin(), entries.end(), entry) != entries.end();
	}
}

TEST(io_read_text_missing_returns_nullopt)
{
	TempDir dir;
	std::error_code ec;

	const auto text = io::read_text(dir.path / "missing.txt", &ec);

	CHECK(!text.has_value());
	CHECK(ec);
}

TEST(io_write_and_read_text_round_trip)
{
	TempDir dir;
	const fs::path file{ dir / "text.txt" };
	const std::string content{ "line one\nline two\r\nutf8: \xE2\x9C\x93\n" };
	std::error_code ec;

	REQUIRE(io::write_text(file, content, true, &ec));
	CHECK(!ec);

	const auto text = io::read_text(file, &ec);

	REQUIRE(text.has_value());
	CHECK(!ec);
	CHECK_EQ(*text, content);
}

TEST(io_write_text_creates_parent_directories)
{
	TempDir dir;
	const fs::path file{ dir / "nested" / "deeper" / "file.txt" };

	CHECK(io::write_text(file, "hello"));

	CHECK(io::is_file(file));
	CHECK(io::is_directory(file.parent_path()));
	CHECK_EQ(*io::read_text(file), std::string("hello"));
}

TEST(io_write_text_can_skip_parent_creation)
{
	TempDir dir;
	const fs::path file{ dir / "missing" / "file.txt" };

	CHECK(!io::write_text(file, "hello", false));
	CHECK(!io::exists(file));
}

TEST(io_write_text_overwrites_existing_file)
{
	TempDir dir;
	const fs::path file{ dir / "file.txt" };

	CHECK(io::write_text(file, "first"));
	CHECK(io::write_text(file, "second"));

	CHECK_EQ(*io::read_text(file), std::string("second"));
}

TEST(io_write_empty_text_creates_file)
{
	TempDir dir;
	const fs::path file{ dir / "empty.txt" };

	CHECK(io::write_text(file, ""));
	CHECK(io::is_file(file));

	const auto text = io::read_text(file);
	REQUIRE(text.has_value());
	CHECK(text->empty());
}

TEST(io_append_text)
{
	TempDir dir;
	const fs::path file{ dir / "append.txt" };

	CHECK(io::append_text(file, "one"));
	CHECK(io::append_text(file, "two"));
	CHECK(io::append_text(file, "three"));

	CHECK_EQ(*io::read_text(file), std::string("onetwothree"));
}

TEST(io_write_text_on_directory_fails)
{
	TempDir dir;
	std::error_code ec;

	CHECK(!io::write_text(dir.path, "data", true, &ec));
	CHECK(ec);
}

TEST(io_binary_round_trip)
{
	TempDir dir;
	const fs::path file{ dir / "data.bin" };
	const std::vector<std::byte> bytes =
		make_bytes({ 0u, 1u, 2u, 127u, 128u, 255u });

	REQUIRE(io::write_binary(file, std::span<const std::byte>(bytes)));

	const auto data = io::read_binary(file);

	REQUIRE(data.has_value());
	CHECK(*data == bytes);
}

TEST(io_binary_write_empty_file)
{
	TempDir dir;
	const fs::path file{ dir / "empty.bin" };

	REQUIRE(io::write_binary(file,
		std::span<const std::byte>()));

	const auto data = io::read_binary(file);

	REQUIRE(data.has_value());
	CHECK(data->empty());
}

TEST(io_append_binary)
{
	TempDir dir;
	const fs::path file{ dir / "append.bin" };
	const std::vector<std::byte> first = make_bytes({ 1u, 2u });
	const std::vector<std::byte> second = make_bytes({ 3u, 4u });

	CHECK(io::append_binary(file, std::span<const std::byte>(first)));
	CHECK(io::append_binary(file, std::span<const std::byte>(second)));

	const auto data = io::read_binary(file);
	REQUIRE(data.has_value());
	CHECK(*data == make_bytes({ 1u, 2u, 3u, 4u }));
}

TEST(io_read_lines_splits_lf_and_crlf)
{
	TempDir dir;

	const fs::path lf{ dir / "lf.txt" };
	const fs::path crlf{ dir / "crlf.txt" };
	const fs::path blanks{ dir / "blanks.txt" };
	const fs::path empty{ dir / "empty.txt" };
	const fs::path onlyNewline{ dir / "newline.txt" };

	REQUIRE(io::write_text(lf, "a\nb\n"));
	REQUIRE(io::write_text(crlf, "a\r\nb\r\n"));
	REQUIRE(io::write_text(blanks, "a\n\nb"));
	REQUIRE(io::write_text(empty, ""));
	REQUIRE(io::write_text(onlyNewline, "\n"));

	const auto lfLines = io::read_lines(lf);
	REQUIRE(lfLines.has_value());
	CHECK_EQ(lfLines->size(), 2u);
	CHECK_EQ((*lfLines)[0], std::string("a"));
	CHECK_EQ((*lfLines)[1], std::string("b"));

	const auto crlfLines = io::read_lines(crlf);
	REQUIRE(crlfLines.has_value());
	CHECK_EQ(crlfLines->size(), 2u);
	CHECK_EQ((*crlfLines)[0], std::string("a"));
	CHECK_EQ((*crlfLines)[1], std::string("b"));

	const auto blankLines = io::read_lines(blanks);
	REQUIRE(blankLines.has_value());
	CHECK_EQ(blankLines->size(), 3u);
	CHECK_EQ((*blankLines)[0], std::string("a"));
	CHECK_EQ((*blankLines)[1], std::string(""));
	CHECK_EQ((*blankLines)[2], std::string("b"));

	const auto emptyLines = io::read_lines(empty);
	REQUIRE(emptyLines.has_value());
	CHECK(emptyLines->empty());

	const auto newlineLines = io::read_lines(onlyNewline);
	REQUIRE(newlineLines.has_value());
	CHECK_EQ(newlineLines->size(), 1u);
	CHECK_EQ((*newlineLines)[0], std::string(""));
}

TEST(io_write_lines)
{
	TempDir dir;
	const fs::path file{ dir / "lines.txt" };
	const std::vector<std::string> lines{ "one", "two", "three" };

	REQUIRE(io::write_lines(file, std::span<const std::string>(lines)));

	CHECK_EQ(*io::read_text(file), std::string("one\ntwo\nthree\n"));

	const auto read = io::read_lines(file);
	REQUIRE(read.has_value());
	CHECK(*read == lines);
}

TEST(io_file_queries)
{
	TempDir dir;
	const fs::path file{ dir / "file.txt" };
	const fs::path sub{ dir / "sub" };

	REQUIRE(io::write_text(file, "hello"));
	REQUIRE(io::create_directory(sub));

	CHECK(io::exists(file));
	CHECK(io::exists(sub));
	CHECK(io::is_file(file));
	CHECK(!io::is_file(sub));
	CHECK(io::is_directory(sub));
	CHECK(!io::is_directory(file));

	const auto size = io::file_size(file);
	REQUIRE(size.has_value());
	CHECK_EQ(*size, 5u);

	CHECK(!io::file_size(dir / "missing.txt").has_value());
	CHECK(io::last_write_time(file).has_value());
}

TEST(io_create_directories)
{
	TempDir dir;
	const fs::path nested{ dir / "one" / "two" / "three" };

	CHECK(io::create_directories(nested));
	CHECK(io::is_directory(nested));
	CHECK(io::create_directories(nested));
}

TEST(io_remove_file)
{
	TempDir dir;
	const fs::path file{ dir / "file.txt" };

	REQUIRE(io::write_text(file, "x"));
	CHECK(io::remove_file(file));
	CHECK(!io::exists(file));

	// Already absent is still success.
	CHECK(io::remove_file(file));

	// Directories are handled by remove_all instead.
	const fs::path sub{ dir / "sub" };
	REQUIRE(io::create_directory(sub));
	CHECK(!io::remove_file(sub));
	CHECK(io::is_directory(sub));
}

TEST(io_remove_all)
{
	TempDir dir;
	const fs::path root{ dir / "tree" };

	REQUIRE(io::write_text(root / "a" / "b" / "file.txt", "x"));
	REQUIRE(io::write_text(root / "top.txt", "y"));

	CHECK(io::remove_all(root));
	CHECK(!io::exists(root));

	// Already absent is still success.
	CHECK(io::remove_all(root));
}

TEST(io_copy_file)
{
	TempDir dir;
	const fs::path source{ dir / "source.txt" };
	const fs::path target{ dir / "target.txt" };
	const fs::path copy{ dir / "copy.txt" };

	REQUIRE(io::write_text(source, "source"));
	REQUIRE(io::write_text(target, "target"));

	CHECK(!io::copy_file(source, target));
	CHECK_EQ(*io::read_text(target), std::string("target"));

	CHECK(io::copy_file(source, target, true));
	CHECK_EQ(*io::read_text(target), std::string("source"));

	CHECK(io::copy_file(source, copy));
	CHECK_EQ(*io::read_text(copy), std::string("source"));
}

TEST(io_rename)
{
	TempDir dir;
	const fs::path source{ dir / "source.txt" };
	const fs::path target{ dir / "target.txt" };
	const fs::path moved{ dir / "moved.txt" };

	REQUIRE(io::write_text(source, "source"));
	REQUIRE(io::write_text(target, "target"));

	CHECK(!io::rename(source, target));
	CHECK(io::exists(source));
	CHECK_EQ(*io::read_text(target), std::string("target"));

	CHECK(io::rename(source, target, true));
	CHECK(!io::exists(source));
	CHECK_EQ(*io::read_text(target), std::string("source"));

	REQUIRE(io::write_text(source, "again"));
	CHECK(io::rename(source, moved));
	CHECK(!io::exists(source));
	CHECK_EQ(*io::read_text(moved), std::string("again"));
}

TEST(io_list_directory)
{
	TempDir dir;
	const fs::path sub{ dir / "sub" };
	const fs::path top{ dir / "top.txt" };
	const fs::path nested{ sub / "nested.txt" };

	REQUIRE(io::create_directory(sub));
	REQUIRE(io::write_text(top, "top"));
	REQUIRE(io::write_text(nested, "nested"));

	const auto flat = io::list_directory(dir.path);
	REQUIRE(flat.has_value());
	CHECK_EQ(flat->size(), 2u);
	CHECK(contains(*flat, top));
	CHECK(contains(*flat, sub));

	const auto deep = io::list_directory(dir.path, true);
	REQUIRE(deep.has_value());
	CHECK_EQ(deep->size(), 3u);
	CHECK(contains(*deep, top));
	CHECK(contains(*deep, sub));
	CHECK(contains(*deep, nested));
}

TEST(io_list_missing_directory_returns_nullopt)
{
	TempDir dir;

	const auto entries = io::list_directory(dir.path / "missing");

	CHECK(!entries.has_value());
}

TEST(io_current_and_temp_directory)
{
	TempDir dir;

	std::error_code ec;
	const auto cwd = io::current_path(&ec);
	REQUIRE(cwd.has_value());
	CHECK(!ec);
	CHECK(io::is_directory(*cwd));

	const auto temp = io::temp_directory(&ec);
	REQUIRE(temp.has_value());
	CHECK(!ec);
	CHECK(io::is_directory(*temp));
}

TEST(io_join_paths)
{
	CHECK_EQ(io::join("root", "sub", "file.txt"),
		fs::path("root") / "sub" / "file.txt");
	CHECK_EQ(io::join("root", "", "file.txt"),
		fs::path("root") / "file.txt");
	CHECK(io::join().empty());

	const std::vector<io::path> parts{ "a", "b", "c" };
	CHECK_EQ(io::join(std::span<const io::path>(parts)),
		fs::path("a") / "b" / "c");
}
