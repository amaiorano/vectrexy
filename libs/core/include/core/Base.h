#pragma once

#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <utility>

// Platform defines
#ifdef _MSC_VER
#define PLATFORM_WINDOWS
#elif __linux__
#define PLATFORM_LINUX
#else
#error "Define current platform"
#endif

//@TODO: Find standard way to detect endianness
#define ENDIANESS_LITTLE 1
#define ENDIANESS_BIG (!ENDIANESS_LITTLE)

// If BITFIELDS_MSB_TO_LSB is 1, then 1-bit bitfields will be laid out in msb to lsb order.
// This is implementation-defined, and in fact is probably not a good idea to rely upon.
#ifdef _MSC_VER
#define BITFIELDS_MSB_TO_LSB 0
#endif

// Build config defines
#if defined(_DEBUG)
#define CONFIG_DEBUG 1
#endif

// Disable warnings
#ifdef _MSC_VER
// nonstandard extension used : nameless struct/union
#pragma warning(disable : 4201)
#endif

// Macros to push/pop warning disables (compiler specific)
#ifdef _MSC_VER
#define MSC_PUSH_WARNING_DISABLE(...)                                                              \
    __pragma(warning(push)) __pragma(warning(disable : __VA_ARGS__))

#define MSC_POP_WARNING_DISABLE() __pragma(warning(pop))
#else
#define MSC_PUSH_WARNING_DISABLE(warning_number)
#define MSC_POP_WARNING_DISABLE()
#endif

// Optimization control
#ifdef _MSC_VER
#define OPTIMIZE_OFF __pragma(optimize("", off))
#define OPTIMIZE_ON __pragma(optimize("", on))
#define NO_INLINE_FUNC __declspec(noinline)
#endif

template <typename T>
constexpr bool IsPowerOfTwo(T value) {
    return (value != 0) && ((value & (value - 1)) == 0);
}

template <typename T, typename U>
T checked_static_cast(U value) {
    assert(static_cast<U>(static_cast<T>(value)) == value && "Cast truncates value");
    return static_cast<T>(value);
}

// BITS macro evaluates to a size_t with the specified bits set
// Example: BITS(0,2,4) evaluates 10101
namespace Internal {
    template <size_t value>
    struct ShiftLeft1 {
        static const size_t Result = 1 << value;
    };
    template <>
    struct ShiftLeft1<~0u> {
        static const size_t Result = 0;
    };

    template <size_t b0, size_t b1 = ~0u, size_t b2 = ~0u, size_t b3 = ~0u, size_t b4 = ~0u,
              size_t b5 = ~0u, size_t b6 = ~0u, size_t b7 = ~0u, size_t b8 = ~0u, size_t b9 = ~0u,
              size_t b10 = ~0u, size_t b11 = ~0u, size_t b12 = ~0u, size_t b13 = ~0u,
              size_t b14 = ~0u, size_t b15 = ~0u>
    struct BitMask {
        static const size_t Result =
            ShiftLeft1<b0>::Result | ShiftLeft1<b1>::Result | ShiftLeft1<b2>::Result |
            ShiftLeft1<b3>::Result | ShiftLeft1<b4>::Result | ShiftLeft1<b5>::Result |
            ShiftLeft1<b6>::Result | ShiftLeft1<b7>::Result | ShiftLeft1<b8>::Result |
            ShiftLeft1<b9>::Result | ShiftLeft1<b10>::Result | ShiftLeft1<b11>::Result |
            ShiftLeft1<b12>::Result | ShiftLeft1<b13>::Result | ShiftLeft1<b14>::Result |
            ShiftLeft1<b15>::Result;
    };
} // namespace Internal
#define BITS(...) Internal::BitMask<__VA_ARGS__>::Result

// Utility for creating a temporary formatted string
template <int MaxLength = 1024>
struct FormattedString {
    FormattedString(const char* format, ...) {
        va_list args;
        va_start(args, format);
        int result = vsnprintf(buffer, MaxLength, format, args);
        // Safety in case string couldn't find completely: make last character a \0
        if (result < 0 || result >= MaxLength) {
            buffer[MaxLength - 1] = 0;
        }
        va_end(args);
    }

    const char* Value() const { return buffer; }

    operator const char*() const { return Value(); }

    char buffer[MaxLength];
};

// ScopedExit RAII class for invoking function (usually lambda expression) on destruction
template <typename Func>
struct ScopedExit {
    ScopedExit(Func func)
        : m_func(std::move(func)) {}
    ~ScopedExit() { m_func(); }
    Func m_func;
};
template <typename Func>
auto MakeScopedExit(Func func) {
    return ScopedExit<Func>(std::move(func));
}

// ASSERT macro
inline void AssertHandler(const char* file, int line, const char* condition, const char* msg) {
    throw std::logic_error(
        FormattedString<>("Assertion Failed!\n Condition: %s\n File: %s(%d)\n Message: %s\n",
                          condition, file, line, msg == nullptr ? "N/A" : msg));
}

// NOTE: Need this helper for clang/gcc so we can pass a single arg to FAIL
#define ASSERT_HELPER(file, line, condition, msg, ...)                                             \
    AssertHandler(file, line, condition, msg ? FormattedString<>(msg, ##__VA_ARGS__) : nullptr)

#define ASSERT(condition)                                                                          \
    (void)((!!(condition)) || (ASSERT_HELPER(__FILE__, (int)__LINE__, #condition, ""), false))

#define ASSERT_MSG(condition, msg, ...)                                                            \
    (void)((!!(condition)) ||                                                                      \
           (ASSERT_HELPER(__FILE__, (int)__LINE__, #condition, msg, ##__VA_ARGS__), false))

// FAIL macro
#define FAIL() ASSERT(false)
#define FAIL_MSG(msg, ...) ASSERT_MSG(false, msg, ##__VA_ARGS__)

// Type aliases
using cycles_t = uint64_t;

template <typename Container, typename T>
bool contains(Container container, T& value) {
    return std::find(begin(container), end(container), value) != end(container);
}

template <typename Container, typename Pred>
bool find_if(const Container& container, Pred pred) {
    return std::find_if(std::begin(container), std::end(container), pred) != std::end(container);
}

// Returns index of value in container, or index_if_not_found
template <typename Container, typename T>
int find_index_of(const Container& container, const T& value, int index_if_not_found = -1) {
    auto b = std::begin(container);
    auto e = std::end(container);
    auto iter = std::find(b, e, value);
    if (iter != e) {
        return static_cast<int>(std::distance(b, iter));
    }
    return index_if_not_found;
}

// Adapter with a variafdic constructor template to forward the arguments, useful for aggregate
// initialization of types via forwarding functions like make_shared.
// From: https://stackoverflow.com/a/35300172/4039972
template <class T>
struct aggregate_adapter : public T {
    template <class... Args>
    aggregate_adapter(Args&&... args)
        : T{std::forward<Args>(args)...} {}
};

// Evaluates to false as a type-dependent expression. Useful for
// static_assert(dependent_false<T>::value) in 'if constexpr' else cases, for e.g., where
// static_assert(false) would always fail. From: https://en.cppreference.com/w/cpp/language/if
template <class T>
struct dependent_false : std::false_type {};
