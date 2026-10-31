#include <wcore/json.hpp>

#include <wcore/assert.hpp>

#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iterator>
#include <limits>
#include <locale>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>

namespace wcore::json
{
	Value::Value() noexcept : mData(nullptr) {}

	Value::Value(std::nullptr_t) noexcept : mData(nullptr) {}

	Value::Value(bool value) : mData(value) {}

	Value::Value(double value) : mData(value) {}

	Value::Value(std::string value) : mData(std::move(value)) {}

	Value::Value(std::string_view value) : mData(std::string(value)) {}

	Value::Value(const char* value)
	{
		WCORE_ASSERT(value != nullptr, "json::Value received a null string");
		mData = std::string(value);
	}

	Value::Value(Array value)
		: mData(std::make_shared<Array>(std::move(value)))
	{
	}

	Value::Value(Object value)
		: mData(std::make_shared<Object>(std::move(value)))
	{
	}

	Value::Value(const Value& other)
	{
		copy_from(other);
	}

	Value::Value(Value&& other) noexcept : mData(std::move(other.mData))
	{
		// Leave the source as null rather than holding a moved-from
		// container pointer or string.
		other.mData = nullptr;
	}

	Value& Value::operator=(const Value& other)
	{
		if (this != &other)
			copy_from(other);

		return *this;
	}

	Value& Value::operator=(Value&& other) noexcept
	{
		if (this != &other)
		{
			mData = std::move(other.mData);
			other.mData = nullptr;
		}

		return *this;
	}

	void Value::copy_from(const Value& other)
	{
		std::visit([this](const auto& value)
		{
			using T = std::decay_t<decltype(value)>;

			if constexpr (std::is_same_v<T, std::shared_ptr<Array>>)
			{
				mData = value ? std::make_shared<Array>(*value)
					: std::make_shared<Array>();
			}
			else if constexpr (std::is_same_v<T, std::shared_ptr<Object>>)
			{
				mData = value ? std::make_shared<Object>(*value)
					: std::make_shared<Object>();
			}
			else
			{
				mData = value;
			}
		}, other.mData);
	}

	Value Value::array()
	{
		return Value(Array{});
	}

	Value Value::object()
	{
		return Value(Object{});
	}

	Type Value::type() const noexcept
	{
		switch (mData.index())
		{
		case 0: return Type::Null;
		case 1: return Type::Boolean;
		case 2: return Type::Integer;
		case 3: return Type::Number;
		case 4: return Type::String;
		case 5: return Type::Array;
		case 6: return Type::Object;
		}

		WCORE_UNREACHABLE("invalid json::Value state");
		return Type::Null;
	}

	bool Value::as_bool() const
	{
		WCORE_ASSERT(is_bool(), "json::Value is not a boolean");
		return std::get<bool>(mData);
	}

	i64 Value::as_i64() const
	{
		WCORE_ASSERT(is_integer(), "json::Value is not an integer");
		return std::get<i64>(mData);
	}

	double Value::as_double() const
	{
		WCORE_ASSERT(is_number(), "json::Value is not a number");

		if (is_integer())
			return static_cast<double>(std::get<i64>(mData));

		return std::get<double>(mData);
	}

	const std::string& Value::as_string() const
	{
		WCORE_ASSERT(is_string(), "json::Value is not a string");
		return std::get<std::string>(mData);
	}

	std::string& Value::as_string()
	{
		WCORE_ASSERT(is_string(), "json::Value is not a string");
		return std::get<std::string>(mData);
	}

	const Array& Value::as_array() const
	{
		WCORE_ASSERT(is_array(), "json::Value is not an array");
		return *std::get<std::shared_ptr<Array>>(mData);
	}

	Array& Value::as_array()
	{
		WCORE_ASSERT(is_array(), "json::Value is not an array");
		return *std::get<std::shared_ptr<Array>>(mData);
	}

	const Object& Value::as_object() const
	{
		WCORE_ASSERT(is_object(), "json::Value is not an object");
		return *std::get<std::shared_ptr<Object>>(mData);
	}

	Object& Value::as_object()
	{
		WCORE_ASSERT(is_object(), "json::Value is not an object");
		return *std::get<std::shared_ptr<Object>>(mData);
	}

	Value& Value::operator[](std::size_t index)
	{
		Array& values = as_array();
		WCORE_ASSERT(index < values.size(), "json array index out of range");
		return values[index];
	}

	const Value& Value::operator[](std::size_t index) const
	{
		const Array& values = as_array();
		WCORE_ASSERT(index < values.size(), "json array index out of range");
		return values[index];
	}

	Value& Value::operator[](std::string_view key)
	{
		Object& values = as_object();
		const auto [it, inserted] = values.try_emplace(std::string(key));
		(void)inserted;
		return it->second;
	}

	const Value& Value::operator[](std::string_view key) const
	{
		const Object& values = as_object();
		const auto it = values.find(key);
		WCORE_ASSERT(it != values.end(), "json object key not found");
		return it->second;
	}

	Value* Value::find(std::string_view key) noexcept
	{
		if (!is_object()) return nullptr;

		Object& values = as_object();
		const auto it = values.find(key);
		return it == values.end() ? nullptr : &it->second;
	}

	const Value* Value::find(std::string_view key) const noexcept
	{
		if (!is_object()) return nullptr;

		const Object& values = as_object();
		const auto it = values.find(key);
		return it == values.end() ? nullptr : &it->second;
	}

	bool Value::contains(std::string_view key) const noexcept
	{
		return find(key) != nullptr;
	}

	Value& Value::push_back(Value value)
	{
		Array& values = as_array();
		values.push_back(std::move(value));
		return values.back();
	}

	std::size_t Value::size() const noexcept
	{
		switch (type())
		{
		case Type::String:
			return std::get<std::string>(mData).size();
		case Type::Array:
			return std::get<std::shared_ptr<Array>>(mData)->size();
		case Type::Object:
			return std::get<std::shared_ptr<Object>>(mData)->size();
		default:
			return 0;
		}
	}

	namespace
	{
		constexpr std::size_t MaxDepth{ 512 };

		bool is_digit(char c) noexcept
		{
			return c >= '0' && c <= '9';
		}

		bool is_whitespace(char c) noexcept
		{
			return c == ' ' || c == '\t' || c == '\n' || c == '\r';
		}

		// Floating-point <charconv> support is missing from older standard
		// libraries (libc++ only ships it as of LLVM 20), so fall back to the
		// classic locale there. Both paths stay locale independent, so the
		// decimal point is always '.'.
		template <typename T>
		concept parseable_with_from_chars = requires (
			const char* first, const char* last, T& value)
		{
			std::from_chars(first, last, value);
		};

		template <typename T>
		concept formattable_with_to_chars = requires (
			char* first, char* last, T value)
		{
			std::to_chars(first, last, value);
		};

		// Parses text as a floating-point value, requiring full consumption.
		template <typename T>
		bool parse_float(std::string_view text, T& value)
		{
			if constexpr (parseable_with_from_chars<T>)
			{
				const auto result = std::from_chars(
					text.data(), text.data() + text.size(), value);

				return result.ec == std::errc{} &&
					result.ptr == text.data() + text.size();
			}
			else
			{
				std::istringstream stream{ std::string(text) };
				stream.imbue(std::locale::classic());

				T parsed{};
				stream >> parsed;

				// Overflow sets failbit, trailing garbage leaves eof unset.
				if (stream.fail() || !stream.eof())
					return false;

				value = parsed;
				return true;
			}
		}

		// Formats a floating-point value with enough digits for a round trip.
		template <typename T>
		std::string format_float(T value)
		{
			if constexpr (formattable_with_to_chars<T>)
			{
				char buffer[64];
				const auto result = std::to_chars(
					buffer, buffer + sizeof(buffer), value);

				WCORE_ASSERT(result.ec == std::errc{},
					"json number formatting failed");

				return std::string(buffer,
					static_cast<std::size_t>(result.ptr - buffer));
			}
			else
			{
				std::ostringstream stream;
				stream.imbue(std::locale::classic());
				stream << std::setprecision(
					std::numeric_limits<T>::max_digits10)
					<< std::defaultfloat << value;
				return stream.str();
			}
		}

		void append_utf8(std::string& out, u32 code)
		{
			if (code <= 0x7F)
			{
				out += static_cast<char>(code);
			}
			else if (code <= 0x7FF)
			{
				out += static_cast<char>(0xC0 | (code >> 6));
				out += static_cast<char>(0x80 | (code & 0x3F));
			}
			else if (code <= 0xFFFF)
			{
				out += static_cast<char>(0xE0 | (code >> 12));
				out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
				out += static_cast<char>(0x80 | (code & 0x3F));
			}
			else
			{
				out += static_cast<char>(0xF0 | (code >> 18));
				out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
				out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
				out += static_cast<char>(0x80 | (code & 0x3F));
			}
		}

		class Parser
		{
		public:
			Parser(std::string_view text, ParseError* error) noexcept
				: mText(text), mError(error)
			{
			}

			std::optional<Value> run()
			{
				skip_bom();
				skip_whitespace();

				if (mPos >= mText.size())
				{
					set_error("empty input");
					return std::nullopt;
				}

				auto value = parse_value(0);
				if (!value) return std::nullopt;

				skip_whitespace();
				if (mPos != mText.size())
				{
					set_error("unexpected trailing characters");
					return std::nullopt;
				}

				return value;
			}

		private:
			void skip_bom() noexcept
			{
				if (mText.size() < 3) return;

				const auto b0 = static_cast<unsigned char>(mText[0]);
				const auto b1 = static_cast<unsigned char>(mText[1]);
				const auto b2 = static_cast<unsigned char>(mText[2]);

				if (b0 == 0xEF && b1 == 0xBB && b2 == 0xBF)
					mPos = 3;
			}

			void skip_whitespace() noexcept
			{
				while (mPos < mText.size() && is_whitespace(mText[mPos]))
					++mPos;
			}

			void set_error(std::string_view message)
			{
				if (!mError) return;

				mError->offset = mPos;

				std::size_t line = 1;
				std::size_t column = 1;

				for (std::size_t i = 0; i < mPos && i < mText.size(); ++i)
				{
					if (mText[i] == '\n')
					{
						++line;
						column = 1;
					}
					else
					{
						++column;
					}
				}

				mError->line = line;
				mError->column = column;
				mError->message.assign(message);
			}

			std::optional<Value> parse_value(std::size_t depth)
			{
				if (depth > MaxDepth)
				{
					set_error("maximum nesting depth exceeded");
					return std::nullopt;
				}

				skip_whitespace();

				if (mPos >= mText.size())
				{
					set_error("unexpected end of input");
					return std::nullopt;
				}

				switch (mText[mPos])
				{
				case '{':
					return parse_object(depth);
				case '[':
					return parse_array(depth);
				case '"':
				{
					auto text = parse_string();
					if (!text) return std::nullopt;
					return Value(std::move(*text));
				}
				case 't':
					return parse_literal("true", Value(true));
				case 'f':
					return parse_literal("false", Value(false));
				case 'n':
					return parse_literal("null", Value(nullptr));
				default:
					if (mText[mPos] == '-' || is_digit(mText[mPos]))
						return parse_number();

					set_error("unexpected character");
					return std::nullopt;
				}
			}

			std::optional<Value> parse_literal(std::string_view literal,
				Value value)
			{
				if (mText.substr(mPos, literal.size()) != literal)
				{
					set_error("invalid literal");
					return std::nullopt;
				}

				mPos += literal.size();
				return value;
			}

			std::optional<Value> parse_object(std::size_t depth)
			{
				++mPos; // '{'

				Object object;
				skip_whitespace();

				if (mPos < mText.size() && mText[mPos] == '}')
				{
					++mPos;
					return Value(std::move(object));
				}

				while (true)
				{
					skip_whitespace();

					if (mPos >= mText.size() || mText[mPos] != '"')
					{
						set_error("expected object key string");
						return std::nullopt;
					}

					auto key = parse_string();
					if (!key) return std::nullopt;

					skip_whitespace();

					if (mPos >= mText.size() || mText[mPos] != ':')
					{
						set_error("expected ':' after object key");
						return std::nullopt;
					}

					++mPos;

					auto value = parse_value(depth + 1);
					if (!value) return std::nullopt;

					// Duplicate keys follow the common last-wins rule.
					object.insert_or_assign(std::move(*key),
						std::move(*value));

					skip_whitespace();

					if (mPos >= mText.size())
					{
						set_error("unterminated object");
						return std::nullopt;
					}

					if (mText[mPos] == ',')
					{
						++mPos;
						continue;
					}

					if (mText[mPos] == '}')
					{
						++mPos;
						break;
					}

					set_error("expected ',' or '}' in object");
					return std::nullopt;
				}

				return Value(std::move(object));
			}

			std::optional<Value> parse_array(std::size_t depth)
			{
				++mPos; // '['

				Array array;
				skip_whitespace();

				if (mPos < mText.size() && mText[mPos] == ']')
				{
					++mPos;
					return Value(std::move(array));
				}

				while (true)
				{
					auto value = parse_value(depth + 1);
					if (!value) return std::nullopt;

					array.push_back(std::move(*value));

					skip_whitespace();

					if (mPos >= mText.size())
					{
						set_error("unterminated array");
						return std::nullopt;
					}

					if (mText[mPos] == ',')
					{
						++mPos;
						continue;
					}

					if (mText[mPos] == ']')
					{
						++mPos;
						break;
					}

					set_error("expected ',' or ']' in array");
					return std::nullopt;
				}

				return Value(std::move(array));
			}

			std::optional<std::string> parse_string()
			{
				// The caller has checked that the current byte is '"'.
				++mPos;

				std::string result;

				while (true)
				{
					if (mPos >= mText.size())
					{
						set_error("unterminated string");
						return std::nullopt;
					}

					const char c = mText[mPos];

					if (c == '"')
					{
						++mPos;
						return result;
					}

					if (static_cast<unsigned char>(c) < 0x20)
					{
						set_error("control character in string");
						return std::nullopt;
					}

					if (c != '\\')
					{
						result += c;
						++mPos;
						continue;
					}

					++mPos;

					if (mPos >= mText.size())
					{
						set_error("unterminated escape sequence");
						return std::nullopt;
					}

					const char escape = mText[mPos++];

					switch (escape)
					{
					case '"': result += '"'; break;
					case '\\': result += '\\'; break;
					case '/': result += '/'; break;
					case 'b': result += '\b'; break;
					case 'f': result += '\f'; break;
					case 'n': result += '\n'; break;
					case 'r': result += '\r'; break;
					case 't': result += '\t'; break;
					case 'u':
						if (!parse_unicode_escape(result))
							return std::nullopt;
						break;
					default:
						set_error("invalid escape sequence");
						return std::nullopt;
					}
				}
			}

			bool parse_hex4(u32& out)
			{
				if (mText.size() - mPos < 4)
				{
					set_error("incomplete \\u escape");
					return false;
				}

				u32 value = 0;

				for (int i = 0; i < 4; ++i)
				{
					const char c = mText[mPos++];
					value <<= 4;

					if (c >= '0' && c <= '9')
						value |= static_cast<u32>(c - '0');
					else if (c >= 'a' && c <= 'f')
						value |= static_cast<u32>(c - 'a' + 10);
					else if (c >= 'A' && c <= 'F')
						value |= static_cast<u32>(c - 'A' + 10);
					else
					{
						set_error("invalid hex digit in \\u escape");
						return false;
					}
				}

				out = value;
				return true;
			}

			bool parse_unicode_escape(std::string& out)
			{
				u32 code = 0;
				if (!parse_hex4(code))
					return false;

				if (code >= 0xD800 && code <= 0xDBFF)
				{
					if (mText.size() - mPos < 2 ||
						mText[mPos] != '\\' || mText[mPos + 1] != 'u')
					{
						set_error(
							"high surrogate without low surrogate");
						return false;
					}

					mPos += 2;

					u32 low = 0;
					if (!parse_hex4(low))
						return false;

					if (low < 0xDC00 || low > 0xDFFF)
					{
						set_error("invalid low surrogate");
						return false;
					}

					code = 0x10000 +
						((code - 0xD800) << 10) + (low - 0xDC00);
				}
				else if (code >= 0xDC00 && code <= 0xDFFF)
				{
					set_error("lone low surrogate");
					return false;
				}

				append_utf8(out, code);
				return true;
			}

			std::optional<Value> parse_number()
			{
				const std::size_t start = mPos;

				if (mText[mPos] == '-')
					++mPos;

				if (mPos >= mText.size())
				{
					set_error("incomplete number");
					return std::nullopt;
				}

				if (mText[mPos] == '0')
				{
					++mPos;
				}
				else if (mText[mPos] >= '1' && mText[mPos] <= '9')
				{
					do
					{
						++mPos;
					} while (mPos < mText.size() && is_digit(mText[mPos]));
				}
				else
				{
					set_error("invalid number");
					return std::nullopt;
				}

				bool integer = true;

				if (mPos < mText.size() && mText[mPos] == '.')
				{
					integer = false;
					++mPos;

					if (mPos >= mText.size() || !is_digit(mText[mPos]))
					{
						set_error(
							"expected digits after decimal point");
						return std::nullopt;
					}

					while (mPos < mText.size() && is_digit(mText[mPos]))
						++mPos;
				}

				if (mPos < mText.size() &&
					(mText[mPos] == 'e' || mText[mPos] == 'E'))
				{
					integer = false;
					++mPos;

					if (mPos < mText.size() &&
						(mText[mPos] == '+' || mText[mPos] == '-'))
					{
						++mPos;
					}

					if (mPos >= mText.size() || !is_digit(mText[mPos]))
					{
						set_error("expected digits in exponent");
						return std::nullopt;
					}

					while (mPos < mText.size() && is_digit(mText[mPos]))
						++mPos;
				}

				const std::string_view token =
					mText.substr(start, mPos - start);

				if (integer)
				{
					i64 value = 0;
					const auto result = std::from_chars(
						token.data(), token.data() + token.size(),
						value);

					if (result.ec == std::errc{} &&
						result.ptr == token.data() + token.size())
					{
						return Value(value);
					}
				}

				double value = 0.0;

				if (!parse_float<double>(token, value))
				{
					set_error("number out of range");
					return std::nullopt;
				}

				return Value(value);
			}

			std::string_view mText;
			std::size_t mPos{ 0 };
			ParseError* mError{ nullptr };
		};

		void escape_string(std::string& out, std::string_view text)
		{
			static constexpr char Hex[] = "0123456789abcdef";

			out += '"';

			for (const char raw : text)
			{
				const auto c = static_cast<unsigned char>(raw);

				switch (c)
				{
				case '"': out += "\\\""; break;
				case '\\': out += "\\\\"; break;
				case '\b': out += "\\b"; break;
				case '\f': out += "\\f"; break;
				case '\n': out += "\\n"; break;
				case '\r': out += "\\r"; break;
				case '\t': out += "\\t"; break;
				default:
					if (c < 0x20)
					{
						out += "\\u00";
						out += Hex[c >> 4];
						out += Hex[c & 0x0F];
					}
					else
					{
						out += raw;
					}
					break;
				}
			}

			out += '"';
		}

		void dump_value(std::string& out, const Value& value,
			const DumpOptions& options, u32 depth)
		{
			switch (value.type())
			{
			case Type::Null:
				out += "null";
				break;

			case Type::Boolean:
				out += value.as_bool() ? "true" : "false";
				break;

			case Type::Integer:
			{
				char buffer[32];
				const auto result = std::to_chars(
					buffer, buffer + sizeof(buffer), value.as_i64());

				WCORE_ASSERT(result.ec == std::errc{},
					"json integer formatting failed");

				out.append(buffer,
					static_cast<std::size_t>(result.ptr - buffer));
				break;
			}

			case Type::Number:
			{
				const double number = value.as_double();

				if (!std::isfinite(number))
				{
					out += "null";
					break;
				}

				const std::string text{ format_float<double>(number) };

				out.append(text);

				// Keep the value a JSON number on a round trip.
				if (text.find('.') == std::string::npos &&
					text.find('e') == std::string::npos &&
					text.find('E') == std::string::npos)
				{
					out += ".0";
				}
				break;
			}

			case Type::String:
				escape_string(out, value.as_string());
				break;

			case Type::Array:
			{
				const Array& values = value.as_array();

				if (values.empty())
				{
					out += "[]";
					break;
				}

				const std::size_t step =
					static_cast<std::size_t>(depth + 1) * options.indent;

				out += '[';

				if (options.pretty)
					out += '\n';

				for (std::size_t i = 0; i < values.size(); ++i)
				{
					if (options.pretty)
						out.append(step, ' ');

					dump_value(out, values[i], options, depth + 1);

					if (i + 1 < values.size())
						out += ',';

					if (options.pretty)
						out += '\n';
				}

				if (options.pretty)
				{
					out.append(
						static_cast<std::size_t>(depth) * options.indent,
						' ');
				}

				out += ']';
				break;
			}

			case Type::Object:
			{
				const Object& values = value.as_object();

				if (values.empty())
				{
					out += "{}";
					break;
				}

				const std::size_t step =
					static_cast<std::size_t>(depth + 1) * options.indent;

				out += '{';

				if (options.pretty)
					out += '\n';

				for (auto it = values.begin(); it != values.end(); ++it)
				{
					if (options.pretty)
						out.append(step, ' ');

					escape_string(out, it->first);
					out += options.pretty ? ": " : ":";
					dump_value(out, it->second, options, depth + 1);

					if (std::next(it) != values.end())
						out += ',';

					if (options.pretty)
						out += '\n';
				}

				if (options.pretty)
				{
					out.append(
						static_cast<std::size_t>(depth) * options.indent,
						' ');
				}

				out += '}';
				break;
			}
			}
		}
	} // anonymous namespace

	std::optional<Value> parse(std::string_view text, ParseError* error)
	{
		if (error) *error = ParseError{};

		Parser parser(text, error);
		return parser.run();
	}

	std::string dump(const Value& value, const DumpOptions& options)
	{
		std::string out;
		dump_value(out, value, options, 0);
		return out;
	}

	std::string dump(const Value& value, bool pretty, u32 indent)
	{
		return dump(value, DumpOptions{ pretty, indent });
	}
} // namespace wcore::json
