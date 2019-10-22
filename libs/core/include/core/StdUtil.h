#pragma once

#include <type_traits>
#include <variant>

// variant_type_index<T, V> returns index of T in V
// e.g. variant_type_index<int, std::variant<float, int, bool>>::value returns 1
template <typename T, typename V>
struct variant_type_index;

template <typename T, typename... Ts>
struct variant_type_index<T, std::variant<Ts...>> {
private:
    static constexpr size_t get() {
        size_t r = 0;
        auto test = [&r](bool b) {
            if (!b)
                ++r;
            return b;
        };
        (test(std::is_same_v<T, Ts>) || ...);
        return r;
    }

public:
    static constexpr size_t value = get();
};

template <typename T, typename V>
inline constexpr size_t variant_type_index_v = variant_type_index<T, V>::value;
