#pragma once

#include <type_traits>
#include <variant>

namespace std_util {

    // variant_type_index<T, V> returns index of T in V
    // e.g. variant_type_index<int, std::variant<float, int, bool>>::value returns 1
    template <typename T, typename V>
    struct variant_type_index;

    namespace detail {
        template <typename>
        struct tag {};

        template <typename T, typename... Ts>
        constexpr size_t get_variant_type_index() {
            return std::variant<tag<Ts>...>(tag<T>()).index();
        }
    } // namespace detail

    template <typename T, typename... Ts>
    struct variant_type_index<T, std::variant<Ts...>>
        : std::integral_constant<size_t, detail::get_variant_type_index<T, Ts...>()> {};

    template <typename T, typename V>
    constexpr size_t variant_type_index_v = variant_type_index<T, V>::value;

} // namespace std_util
