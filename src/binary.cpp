#include <wcore/binary.hpp>

#include <limits>
#include <string>

#include <wcore/assert.hpp>

namespace wcore::binary
{
	void Writer::write_bytes(ByteView bytes)
	{
		mBytes.insert(mBytes.end(), bytes.begin(), bytes.end());
	}

	void Writer::write_string(std::string_view text)
	{
		WCORE_ASSERT(
			text.size() <= std::numeric_limits<u32>::max(),
			"binary::Writer string is too long");

		write(static_cast<u32>(text.size()));
		if (!text.empty())
		{
			const auto* first =
				reinterpret_cast<const Byte*>(text.data());
			mBytes.insert(mBytes.end(), first, first + text.size());
		}
	}

	void Writer::write_varuint(u64 value)
	{
		while (value >= 0x80)
		{
			mBytes.push_back(
				static_cast<Byte>((value & 0x7F) | 0x80));
			value >>= 7;
		}

		mBytes.push_back(static_cast<Byte>(value));
	}

	void Writer::write_varint(i64 value)
	{
		write_varuint(zigzag_encode(value));
	}

	std::optional<ByteView> Reader::read_bytes(std::size_t count)
	{
		if (!ensure(count)) return std::nullopt;

		const ByteView result{ mBytes.subspan(mPos, count) };
		mPos += count;
		return result;
	}

	std::optional<std::string> Reader::read_string()
	{
		const auto length = read<u32>();
		if (!length) return std::nullopt;

		if (*length > remaining())
		{
			mFailed = true;
			return std::nullopt;
		}

		std::string result;
		if (*length != 0)
		{
			result.assign(
				reinterpret_cast<const char*>(mBytes.data() + mPos),
				*length);
		}

		mPos += *length;
		return result;
	}

	std::optional<u64> Reader::read_varuint()
	{
		u64 result{ 0 };
		unsigned shift{ 0 };

		for (unsigned i{ 0 }; i < 10; ++i)
		{
			const auto byte = read<u8>();
			if (!byte) return std::nullopt;

			const u64 payload{ static_cast<u64>(*byte & 0x7F) };

			// The tenth byte may only carry the final bit.
			if (i == 9 && payload > 1)
			{
				mFailed = true;
				return std::nullopt;
			}

			result |= payload << shift;

			if ((*byte & 0x80) == 0)
				return result;

			shift += 7;
		}

		mFailed = true;
		return std::nullopt;
	}

	std::optional<i64> Reader::read_varint()
	{
		const auto raw = read_varuint();
		if (!raw) return std::nullopt;

		return zigzag_decode(*raw);
	}

	bool Reader::skip(std::size_t count)
	{
		if (!ensure(count)) return false;

		mPos += count;
		return true;
	}

	bool Reader::ensure(std::size_t count) noexcept
	{
		if (mFailed || count > remaining())
		{
			mFailed = true;
			return false;
		}

		return true;
	}
}