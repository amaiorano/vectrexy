#pragma once

#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <cassert>
#include <cstdarg>
#include <stdexcept>

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

