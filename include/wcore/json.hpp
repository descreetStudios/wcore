#pragma once

#include <cstddef>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include <wcore/types.hpp>

namespace wcore::json
{
	struct Value;

	using Array = std::vector<Value>;
	using Object = std::map<std::string, Value, std::less<>>;

	enum class Type : u8
	{
		Null,
		Boolean,
		Integer,
		Number,
		String,
		Array,
		Object
	};

	// A JSON value with value semantics. Arrays and objects are copied
	// deeply, so mutating a copy never affects the original.
	struct Value
	{
		using Storage = std::variant<
			std::nullptr_t,
			bool,
			i64,
			double,
			std::string,
			std::shared_ptr<Array>,
			std::shared_ptr<Object>>;

		Value() noexcept;
		Value(std::nullptr_t) noexcept;
		Value(bool value);
		Value(double value);
		Value(std::string value);
		Value(std::string_view value);
		Value(const char* value);
		Value(Array value);
		Value(Object value);

		// Integral values are stored as i64 when they fit. Larger unsigned
		// values fall back to double instead of wrapping.
		template <typename T>
			requires (std::is_integral_v<T> &&
				!std::is_same_v<std::remove_cv_t<T>, bool>)
		Value(T value)
		{
			if constexpr (std::is_unsigned_v<T> &&
				sizeof(T) >= sizeof(i64))
			{
				if (value > static_cast<u64>(
					std::numeric_limits<i64>::max()))
				{
					mData = static_cast<double>(value);
					return;
				}
			}

			mData = static_cast<i64>(value);
		}

		Value(const Value& other);
		Value(Value&& other) noexcept;
		Value& operator=(const Value& other);
		Value& operator=(Value&& other) noexcept;

		[[nodiscard]] static Value array();
		[[nodiscard]] static Value object();

		[[nodiscard]] Type type() const noexcept;

		[[nodiscard]] bool is_null() const noexcept
		{
			return type() == Type::Null;
		}
		[[nodiscard]] bool is_bool() const noexcept
		{
			return type() == Type::Boolean;
		}
		[[nodiscard]] bool is_integer() const noexcept
		{
			return type() == Type::Integer;
		}
		[[nodiscard]] bool is_number() const noexcept
		{
			const Type t = type();
			return t == Type::Integer || t == Type::Number;
		}
		[[nodiscard]] bool is_string() const noexcept
		{
			return type() == Type::String;
		}
		[[nodiscard]] bool is_array() const noexcept
		{
			return type() == Type::Array;
		}
		[[nodiscard]] bool is_object() const noexcept
		{
			return type() == Type::Object;
		}

		[[nodiscard]] bool as_bool() const;
		[[nodiscard]] i64 as_i64() const;
		[[nodiscard]] double as_double() const;
		[[nodiscard]] const std::string& as_string() const;
		[[nodiscard]] std::string& as_string();

		[[nodiscard]] const Array& as_array() const;
		[[nodiscard]] Array& as_array();
		[[nodiscard]] const Object& as_object() const;
		[[nodiscard]] Object& as_object();

		// Arrays return their element, objects create a null entry when the
		// key is missing. Other types abort.
		[[nodiscard]] Value& operator[](std::size_t index);
		[[nodiscard]] const Value& operator[](std::size_t index) const;

		// Arrays return their element, objects create a null entry when the
		// key is missing. Other types abort.
		[[nodiscard]] Value& operator[](std::string_view key);
		[[nodiscard]] const Value& operator[](std::string_view key) const;

		// Returns nullptr when this is not an object or the key is absent.
		[[nodiscard]] Value* find(std::string_view key) noexcept;
		[[nodiscard]] const Value* find(std::string_view key) const noexcept;
		[[nodiscard]] bool contains(std::string_view key) const noexcept;

		Value& push_back(Value value);

		// String length, array size, or object size. Other types return 0.
		[[nodiscard]] std::size_t size() const noexcept;
		[[nodiscard]] bool empty() const noexcept { return size() == 0; }

	private:
		void copy_from(const Value& other);

		Storage mData;
	};

	struct ParseError
	{
		std::size_t offset{ 0 };
		std::size_t line{ 1 };
		std::size_t column{ 1 };
		std::string message;
	};

	// Returns nullopt on invalid input and fills *error (when non-null).
	[[nodiscard]] std::optional<Value> parse(
		std::string_view text, ParseError* error = nullptr);

	struct DumpOptions
	{
		bool pretty{ false };
		u32 indent{ 2 };
	};

	// Non-finite numbers are written as null, since JSON has no
	// representation for them.
	[[nodiscard]] std::string dump(const Value& value,
		const DumpOptions& options);

	[[nodiscard]] std::string dump(const Value& value,
		bool pretty = false, u32 indent = 2);
} // namespace wcore::json
