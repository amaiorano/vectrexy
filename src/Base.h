#pragma once

#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <cassert>
#include <cstdarg>
#include <stdexcept>
#include <utility>

// Build config defines
#if defined(_DEBUG)
#define CONFIG_DEBUG 1
#endif

// Disable warnings
#if _MSC_VER
#pragma warning(disable : 4201) // nonstandard extension used : nameless struct/union
#endif

template <typename T>
constexpr bool IsPowerOfTwo(T value)
{
	return (value != 0) && ((value & (value - 1)) == 0);
}

template <typename T, typename U>
T checked_static_cast(U value)
{
	assert(static_cast<U>(static_cast<T>(value)) == value && "Cast truncates value");
	return static_cast<T>(value);
}

// BITS macro evaluates to a size_t with the specified bits set
// Example: BITS(0,2,4) evaluates 10101
namespace Internal
{
	template <size_t value> struct ShiftLeft1 { static const size_t Result = 1 << value; };
	template <> struct ShiftLeft1<~0u> { static const size_t Result = 0; };

	template <
		size_t b0,
		size_t b1 = ~0u,
		size_t b2 = ~0u,
		size_t b3 = ~0u,
		size_t b4 = ~0u,
		size_t b5 = ~0u,
		size_t b6 = ~0u,
		size_t b7 = ~0u,
		size_t b8 = ~0u,
		size_t b9 = ~0u,
		size_t b10 = ~0u,
		size_t b11 = ~0u,
		size_t b12 = ~0u,
		size_t b13 = ~0u,
		size_t b14 = ~0u,
		size_t b15 = ~0u
	>
	struct BitMask
	{
		static const size_t Result =
			ShiftLeft1<b0>::Result |
			ShiftLeft1<b1>::Result |
			ShiftLeft1<b2>::Result |
			ShiftLeft1<b3>::Result |
			ShiftLeft1<b4>::Result |
			ShiftLeft1<b5>::Result |
			ShiftLeft1<b6>::Result |
			ShiftLeft1<b7>::Result |
			ShiftLeft1<b8>::Result |
			ShiftLeft1<b9>::Result |
			ShiftLeft1<b10>::Result |
			ShiftLeft1<b11>::Result |
			ShiftLeft1<b12>::Result |
			ShiftLeft1<b13>::Result |
			ShiftLeft1<b14>::Result |
			ShiftLeft1<b15>::Result;
	};
}
#define BITS(...) Internal::BitMask<__VA_ARGS__>::Result

// Utility for creating a temporary formatted string
template <int MaxLength = 1024>
struct FormattedString
{
	FormattedString(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		int result = vsnprintf(buffer, MaxLength, format, args);
		// Safety in case string couldn't find completely: make last character a \0
		if (result < 0 || result >= MaxLength)
		{
			buffer[MaxLength - 1] = 0;
		}
		va_end(args);
	}

	const char* Value() const { return buffer; }

	operator const char*() const { return Value(); }

	char buffer[MaxLength];
};

// FAIL macro

inline void FailHandler(const char* msg)
{
#if CONFIG_DEBUG
	printf("FAIL: %s\n", msg);
#endif
	throw std::logic_error(msg);
}

// NOTE: Need this helper for clang/gcc so we can pass a single arg to FAIL
#define FAIL_HELPER(msg, ...) FailHandler(FormattedString<>(msg, __VA_ARGS__))
#define FAIL(...) FAIL_HELPER(__VA_ARGS__, "")

