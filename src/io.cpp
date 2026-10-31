#include <wcore/io.hpp>

#include <wcore/assert.hpp>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <limits>
#include <system_error>

namespace wcore::io
{
	namespace
	{
		std::error_code io_error() noexcept
		{
			return std::make_error_code(std::errc::io_error);
		}

		void set_error(std::error_code* ec, std::error_code error) noexcept
		{
			if (ec) *ec = error;
		}

		void clear_error(std::error_code* ec) noexcept
		{
			if (ec) ec->clear();
		}

		std::error_code open_error(const path& file) noexcept
		{
			std::error_code ec;

			if (!std::filesystem::exists(file, ec))
				return std::make_error_code(
					std::errc::no_such_file_or_directory);

			if (std::filesystem::is_directory(file, ec))
				return std::make_error_code(std::errc::is_a_directory);

			return io_error();
		}

		bool ensure_parent_directory(const path& file,
			std::error_code* ec)
		{
			const path parent{ file.parent_path() };
			if (parent.empty())
			{
				clear_error(ec);
				return true;
			}

			std::error_code local;
			std::filesystem::create_directories(parent, local);
			if (local)
			{
				set_error(ec, local);
				return false;
			}

			clear_error(ec);
			return true;
		}

		template <typename Container>
		std::optional<Container> read_raw(const path& file,
			std::error_code* ec)
		{
			std::ifstream input(file, std::ios::binary);
			if (!input)
			{
				set_error(ec, open_error(file));
				return std::nullopt;
			}

			input.seekg(0, std::ios::end);
			const std::streamoff end{ input.tellg() };
			if (end < 0 || !input)
			{
				set_error(ec, io_error());
				return std::nullopt;
			}

			input.seekg(0, std::ios::beg);
			if (!input)
			{
				set_error(ec, io_error());
				return std::nullopt;
			}

			const auto length{ static_cast<std::uintmax_t>(end) };
			if (length > std::numeric_limits<std::size_t>::max() ||
				length > static_cast<std::uintmax_t>(
					std::numeric_limits<std::streamsize>::max()))
			{
				set_error(ec,
					std::make_error_code(std::errc::value_too_large));
				return std::nullopt;
			}

			Container data(static_cast<std::size_t>(length),
				typename Container::value_type{});
			if (!data.empty())
			{
				input.read(reinterpret_cast<char*>(data.data()),
					static_cast<std::streamsize>(data.size()));
				if (!input)
				{
					set_error(ec, io_error());
					return std::nullopt;
				}
			}

			clear_error(ec);
			return data;
		}

		bool write_raw(const path& file, const void* data, std::size_t size,
			bool append, bool create_parent_directories,
			std::error_code* ec)
		{
			WCORE_ASSERT(data != nullptr || size == 0,
				"io::write_raw received a null buffer");

			if (create_parent_directories &&
				!ensure_parent_directory(file, ec))
			{
				return false;
			}

			if (size > static_cast<std::size_t>(
				std::numeric_limits<std::streamsize>::max()))
			{
				set_error(ec,
					std::make_error_code(std::errc::value_too_large));
				return false;
			}

			const std::ios::openmode mode{
				std::ios::binary |
				(append ? std::ios::app : std::ios::trunc) };

			std::ofstream output(file, mode);
			if (!output)
			{
				set_error(ec, open_error(file));
				return false;
			}

			if (size != 0)
			{
				output.write(static_cast<const char*>(data),
					static_cast<std::streamsize>(size));
				if (!output)
				{
					set_error(ec, io_error());
					return false;
				}
			}

			output.flush();
			if (!output)
			{
				set_error(ec, io_error());
				return false;
			}

			clear_error(ec);
			return true;
		}

		Lines split_lines(std::string_view text)
		{
			Lines lines;

			std::size_t start{ 0 };
			while (start < text.size())
			{
				const std::size_t end{ text.find('\n', start) };
				std::string_view line;

				if (end == std::string_view::npos)
				{
					line = text.substr(start);
					start = text.size();
				}
				else
				{
					line = text.substr(start, end - start);
					start = end + 1;
				}

				if (!line.empty() && line.back() == '\r')
					line.remove_suffix(1);

				lines.emplace_back(line);
			}

			return lines;
		}
	} // namespace

	std::optional<std::string> read_text(const path& file,
		std::error_code* ec)
	{
		return read_raw<std::string>(file, ec);
	}

	bool write_text(const path& file, std::string_view text,
		bool create_parent_directories, std::error_code* ec)
	{
		return write_raw(file, text.data(), text.size(), false,
			create_parent_directories, ec);
	}

	bool append_text(const path& file, std::string_view text,
		bool create_parent_directories, std::error_code* ec)
	{
		return write_raw(file, text.data(), text.size(), true,
			create_parent_directories, ec);
	}

	std::optional<Lines> read_lines(const path& file, std::error_code* ec)
	{
		const auto text{ read_text(file, ec) };
		if (!text) return std::nullopt;

		return split_lines(*text);
	}

	bool write_lines(const path& file, std::span<const std::string> lines,
		bool create_parent_directories, std::error_code* ec)
	{
		std::size_t total{ 0 };
		for (const std::string& line : lines)
			total += line.size() + 1;

		std::string text;
		text.reserve(total);

		for (const std::string& line : lines)
		{
			text += line;
			text += '\n';
		}

		return write_text(file, text, create_parent_directories, ec);
	}

	std::optional<Bytes> read_binary(const path& file, std::error_code* ec)
	{
		return read_raw<Bytes>(file, ec);
	}

	bool write_binary(const path& file, std::span<const std::byte> data,
		bool create_parent_directories, std::error_code* ec)
	{
		return write_raw(file, data.data(), data.size(), false,
			create_parent_directories, ec);
	}

	bool append_binary(const path& file, std::span<const std::byte> data,
		bool create_parent_directories, std::error_code* ec)
	{
		return write_raw(file, data.data(), data.size(), true,
			create_parent_directories, ec);
	}

	bool exists(const path& entry, std::error_code* ec) noexcept
	{
		std::error_code local;
		const bool result{ std::filesystem::exists(entry, local) };
		if (local)
		{
			set_error(ec, local);
			return false;
		}

		clear_error(ec);
		return result;
	}

	bool is_file(const path& entry, std::error_code* ec) noexcept
	{
		std::error_code local;
		const bool result{ std::filesystem::is_regular_file(entry, local) };
		if (local)
		{
			set_error(ec, local);
			return false;
		}

		clear_error(ec);
		return result;
	}

	bool is_directory(const path& entry, std::error_code* ec) noexcept
	{
		std::error_code local;
		const bool result{ std::filesystem::is_directory(entry, local) };
		if (local)
		{
			set_error(ec, local);
			return false;
		}

		clear_error(ec);
		return result;
	}

	std::optional<u64> file_size(const path& file,
		std::error_code* ec) noexcept
	{
		std::error_code local;
		const std::uintmax_t size{ std::filesystem::file_size(file, local) };
		if (local)
		{
			set_error(ec, local);
			return std::nullopt;
		}

		clear_error(ec);
		return static_cast<u64>(size);
	}

	std::optional<file_time> last_write_time(const path& entry,
		std::error_code* ec) noexcept
	{
		std::error_code local;
		const file_time time{ std::filesystem::last_write_time(entry, local) };
		if (local)
		{
			set_error(ec, local);
			return std::nullopt;
		}

		clear_error(ec);
		return time;
	}

	bool create_directory(const path& directory, std::error_code* ec) noexcept
	{
		std::error_code local;
		std::filesystem::create_directory(directory, local);
		if (local)
		{
			set_error(ec, local);
			return false;
		}

		clear_error(ec);
		return true;
	}

	bool create_directories(const path& directory,
		std::error_code* ec) noexcept
	{
		std::error_code local;
		std::filesystem::create_directories(directory, local);
		if (local)
		{
			set_error(ec, local);
			return false;
		}

		clear_error(ec);
		return true;
	}

	bool remove_file(const path& file, std::error_code* ec) noexcept
	{
		std::error_code local;
		const std::filesystem::file_status status{
			std::filesystem::symlink_status(file, local) };
		if (local)
		{
			// A missing path is already in the desired state.
			if (status.type() == std::filesystem::file_type::not_found)
			{
				clear_error(ec);
				return true;
			}

			set_error(ec, local);
			return false;
		}

		if (std::filesystem::is_directory(status))
		{
			set_error(ec, std::make_error_code(std::errc::is_a_directory));
			return false;
		}

		std::filesystem::remove(file, local);
		if (local)
		{
			set_error(ec, local);
			return false;
		}

		clear_error(ec);
		return true;
	}

	bool remove_all(const path& entry, std::error_code* ec) noexcept
	{
		std::error_code local;
		std::filesystem::remove_all(entry, local);
		if (local)
		{
			set_error(ec, local);
			return false;
		}

		clear_error(ec);
		return true;
	}

	bool copy_file(const path& from, const path& to, bool overwrite,
		std::error_code* ec) noexcept
	{
		std::error_code local;
		const std::filesystem::copy_options options{
			overwrite ? std::filesystem::copy_options::overwrite_existing
				: std::filesystem::copy_options::none };

		const bool copied{
			std::filesystem::copy_file(from, to, options, local) };

		if (local)
		{
			set_error(ec, local);
			return false;
		}

		clear_error(ec);
		return copied;
	}

	bool rename(const path& from, const path& to, bool overwrite,
		std::error_code* ec) noexcept
	{
		if (from == to)
		{
			clear_error(ec);
			return true;
		}

		std::error_code local;

		if (!overwrite)
		{
			const bool target_exists{ std::filesystem::exists(to, local) };
			if (local)
			{
				set_error(ec, local);
				return false;
			}

			if (target_exists)
			{
				set_error(ec, std::make_error_code(std::errc::file_exists));
				return false;
			}
		}
		else
		{
			std::filesystem::remove(to, local);
			if (local)
			{
				set_error(ec, local);
				return false;
			}
		}

		std::filesystem::rename(from, to, local);
		if (local)
		{
			set_error(ec, local);
			return false;
		}

		clear_error(ec);
		return true;
	}

	std::optional<std::vector<path>> list_directory(const path& directory,
		bool recursive, std::error_code* ec)
	{
		std::vector<path> entries;
		std::error_code local;

		if (recursive)
		{
			std::filesystem::recursive_directory_iterator it(
				directory, local);
			const std::filesystem::recursive_directory_iterator end;

			if (local)
			{
				set_error(ec, local);
				return std::nullopt;
			}

			for (; it != end; it.increment(local))
			{
				if (local)
				{
					set_error(ec, local);
					return std::nullopt;
				}

				entries.push_back(it->path());
			}
		}
		else
		{
			std::filesystem::directory_iterator it(directory, local);
			const std::filesystem::directory_iterator end;

			if (local)
			{
				set_error(ec, local);
				return std::nullopt;
			}

			for (; it != end; it.increment(local))
			{
				if (local)
				{
					set_error(ec, local);
					return std::nullopt;
				}

				entries.push_back(it->path());
			}
		}

		if (local)
		{
			set_error(ec, local);
			return std::nullopt;
		}

		std::sort(entries.begin(), entries.end());

		clear_error(ec);
		return entries;
	}

	std::optional<path> current_path(std::error_code* ec) noexcept
	{
		std::error_code local;
		const path result{ std::filesystem::current_path(local) };
		if (local)
		{
			set_error(ec, local);
			return std::nullopt;
		}

		clear_error(ec);
		return result;
	}

	std::optional<path> temp_directory(std::error_code* ec) noexcept
	{
		std::error_code local;
		const path result{ std::filesystem::temp_directory_path(local) };
		if (local)
		{
			set_error(ec, local);
			return std::nullopt;
		}

		clear_error(ec);
		return result;
	}

	path join(std::span<const path> parts)
	{
		path result;

		for (const path& part : parts)
		{
			if (part.empty()) continue;
			if (result.empty()) result = part;
			else result /= part;
		}

		return result;
	}
} // namespace wcore::io
