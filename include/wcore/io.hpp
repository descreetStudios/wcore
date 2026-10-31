#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

#include <wcore/types.hpp>

namespace wcore::io
{
	using path = std::filesystem::path;
	using file_time = std::filesystem::file_time_type;
	using Bytes = std::vector<std::byte>;
	using Lines = std::vector<std::string>;

	[[nodiscard]] std::optional<std::string> read_text(
		const path& file, std::error_code* ec = nullptr);

	bool write_text(const path& file, std::string_view text,
		bool createParentDirectories = true, std::error_code* ec = nullptr);

	bool append_text(const path& file, std::string_view text,
		bool createParentDirectories = true, std::error_code* ec = nullptr);

	// Splits on '\n' and strips one trailing '\r' per line, so both LF and
	// CRLF files work. A trailing newline does not produce an empty last line.
	[[nodiscard]] std::optional<Lines> read_lines(
		const path& file, std::error_code* ec = nullptr);

	// Writes one line per entry, always terminating each line with '\n'.
	bool write_lines(const path& file, std::span<const std::string> lines,
		bool createParentDirectories = true, std::error_code* ec = nullptr);

	[[nodiscard]] std::optional<Bytes> read_binary(
		const path& file, std::error_code* ec = nullptr);

	bool write_binary(const path& file, std::span<const std::byte> data,
		bool createParentDirectories = true, std::error_code* ec = nullptr);

	bool append_binary(const path& file, std::span<const std::byte> data,
		bool createParentDirectories = true, std::error_code* ec = nullptr);

	[[nodiscard]] bool exists(const path& entry,
		std::error_code* ec = nullptr) noexcept;

	[[nodiscard]] bool is_file(const path& entry,
		std::error_code* ec = nullptr) noexcept;

	[[nodiscard]] bool is_directory(const path& entry,
		std::error_code* ec = nullptr) noexcept;

	[[nodiscard]] std::optional<u64> file_size(const path& file,
		std::error_code* ec = nullptr) noexcept;

	[[nodiscard]] std::optional<file_time> last_write_time(const path& entry,
		std::error_code* ec = nullptr) noexcept;

	bool create_directory(const path& directory,
		std::error_code* ec = nullptr) noexcept;

	bool create_directories(const path& directory,
		std::error_code* ec = nullptr) noexcept;

	bool remove_file(const path& file,
		std::error_code* ec = nullptr) noexcept;

	bool remove_all(const path& entry,
		std::error_code* ec = nullptr) noexcept;

	bool copy_file(const path& from, const path& to, bool overwrite = false,
		std::error_code* ec = nullptr) noexcept;

	// Renames a file or directory. With overwrite, an existing destination is
	// removed first so behavior is consistent across platforms.
	bool rename(const path& from, const path& to, bool overwrite = false,
		std::error_code* ec = nullptr) noexcept;

	// Returns directory entries sorted by path. With recursive, nested
	// entries are included as well.
	[[nodiscard]] std::optional<std::vector<path>> list_directory(
		const path& directory, bool recursive = false,
		std::error_code* ec = nullptr);

	[[nodiscard]] std::optional<path> current_path(
		std::error_code* ec = nullptr) noexcept;

	[[nodiscard]] std::optional<path> temp_directory(
		std::error_code* ec = nullptr) noexcept;

	[[nodiscard]] path join(std::span<const path> parts);

	// Joins any number of path-like components, ignoring empty ones.
	template <typename... Parts>
		requires (std::is_constructible_v<path, Parts&&> && ...)
	[[nodiscard]] path join(Parts&&... parts)
	{
		path result;

		auto append = [&result](const path& part)
		{
			if (part.empty()) return;
			if (result.empty()) result = part;
			else result /= part;
		};

		(append(path(std::forward<Parts>(parts))), ...);
		return result;
	}
}
