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
	using Path = std::filesystem::path;
	using FileTime = std::filesystem::file_time_type;
	using Bytes = std::vector<std::byte>;
	using Lines = std::vector<std::string>;

	[[nodiscard]] std::optional<std::string> read_text(
		const Path& file, std::error_code* ec = nullptr);

	bool write_text(const Path& file, std::string_view text,
		bool createParentDirectories = true, std::error_code* ec = nullptr);

	bool append_text(const Path& file, std::string_view text,
		bool createParentDirectories = true, std::error_code* ec = nullptr);

	[[nodiscard]] std::optional<Lines> read_lines(
		const Path& file, std::error_code* ec = nullptr);

	bool write_lines(const Path& file, std::span<const std::string> lines,
		bool createParentDirectories = true, std::error_code* ec = nullptr);

	[[nodiscard]] std::optional<Bytes> read_binary(
		const Path& file, std::error_code* ec = nullptr);

	bool write_binary(const Path& file, std::span<const std::byte> data,
		bool createParentDirectories = true, std::error_code* ec = nullptr);

	bool append_binary(const Path& file, std::span<const std::byte> data,
		bool createParentDirectories = true, std::error_code* ec = nullptr);

	[[nodiscard]] bool exists(const Path& entry,
		std::error_code* ec = nullptr) noexcept;

	[[nodiscard]] bool is_file(const Path& entry,
		std::error_code* ec = nullptr) noexcept;

	[[nodiscard]] bool is_directory(const Path& entry,
		std::error_code* ec = nullptr) noexcept;

	[[nodiscard]] std::optional<u64> file_size(const Path& file,
		std::error_code* ec = nullptr) noexcept;

	[[nodiscard]] std::optional<FileTime> last_write_time(const Path& entry,
		std::error_code* ec = nullptr) noexcept;

	bool create_directory(const Path& directory,
		std::error_code* ec = nullptr) noexcept;

	bool create_directories(const Path& directory,
		std::error_code* ec = nullptr) noexcept;

	bool remove_file(const Path& file,
		std::error_code* ec = nullptr) noexcept;

	bool remove_all(const Path& entry,
		std::error_code* ec = nullptr) noexcept;

	bool copy_file(const Path& from, const Path& to, bool overwrite = false,
		std::error_code* ec = nullptr) noexcept;

	bool rename(const Path& from, const Path& to, bool overwrite = false,
		std::error_code* ec = nullptr) noexcept;

	[[nodiscard]] std::optional<std::vector<Path>> list_directory(
		const Path& directory, bool recursive = false,
		std::error_code* ec = nullptr);

	[[nodiscard]] std::optional<Path> current_path(
		std::error_code* ec = nullptr) noexcept;

	[[nodiscard]] std::optional<Path> temp_directory(
		std::error_code* ec = nullptr) noexcept;

	[[nodiscard]] Path join(std::span<const Path> parts);

	template <typename... Parts>
		requires (std::is_constructible_v<Path, Parts&&> && ...)
	[[nodiscard]] Path join(Parts&&... parts)
	{
		Path result;

		auto append = [&result](const Path& part)
		{
			if (part.empty()) return;
			if (result.empty()) result = part;
			else result /= part;
		};

		(append(Path(std::forward<Parts>(parts))), ...);
		return result;
	}
}
