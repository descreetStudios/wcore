#pragma once

#include <format>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace wcore
{
	using glm::ivec2, glm::ivec3, glm::ivec4;
	using glm::uvec2, glm::uvec3, glm::uvec4;
	using glm::vec2, glm::vec3, glm::vec4;
	using glm::dvec2, glm::dvec3, glm::dvec4;

	using glm::mat2, glm::mat3, glm::mat4;
	using glm::dmat2, glm::dmat3, glm::dmat4;

	using glm::quat, glm::dquat;

	using glm::radians, glm::degrees;

	// Belong here because of glm wrapper.
	using glm::perspective, glm::ortho;

	using glm::pi, glm::half_pi, glm::two_pi;
}

template <glm::length_t L, typename T, glm::qualifier Q>
struct std::formatter<glm::vec<L, T, Q>>
{
private:
	std::formatter<T> element;

public:
	constexpr auto parse(std::format_parse_context& ctx)
	{
		return element.parse(ctx);
	}

	auto format(const glm::vec<L, T, Q>& v, std::format_context& ctx) const
	{
		ctx.out() = std::format_to(ctx.out(), "(");
		for (glm::length_t i{ 0 }; i < L; ++i)
		{
			if (i != 0) ctx.out() = std::format_to(ctx.out(), ", ");
			ctx.out() = element.format(v[i], ctx);
		}
		return std::format_to(ctx.out(), ")");
	}
};

template <glm::length_t C, glm::length_t R, typename T, glm::qualifier Q>
struct std::formatter<glm::mat<C, R, T, Q>>
{
private:
	std::formatter<T> element;

public:
	constexpr auto parse(std::format_parse_context& ctx)
	{
		return element.parse(ctx);
	}

	auto format(const glm::mat<C, R, T, Q>& m, std::format_context& ctx) const
	{
		ctx.out() = std::format_to(ctx.out(), "[");
		for (glm::length_t r{ 0 }; r < R; ++r)
		{
			if (r != 0) ctx.out() = std::format_to(ctx.out(), "; ");
			for (glm::length_t c{ 0 }; c < C; ++c)
			{
				if (c != 0) ctx.out() = std::format_to(ctx.out(), ", ");
				ctx.out() = element.format(m[c][r], ctx);
			}
		}
		return std::format_to(ctx.out(), "]");
	}
};

template <typename T, glm::qualifier Q>
struct std::formatter<glm::qua<T, Q>>
{
private:
	std::formatter<T> element;

public:
	constexpr auto parse(std::format_parse_context& ctx)
	{
		return element.parse(ctx);
	}

	auto format(const glm::qua<T, Q>& q, std::format_context& ctx) const
	{
		ctx.out() = std::format_to(ctx.out(), "(");
		ctx.out() = element.format(q.w, ctx);
		ctx.out() = std::format_to(ctx.out(), ", ");
		ctx.out() = element.format(q.x, ctx);
		ctx.out() = std::format_to(ctx.out(), ", ");
		ctx.out() = element.format(q.y, ctx);
		ctx.out() = std::format_to(ctx.out(), ", ");
		ctx.out() = element.format(q.z, ctx);
		return std::format_to(ctx.out(), ")");
	}
};