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

    // overloaded creates a type derives from all Ts, and brings in operator() from all of them into
    // this type.
    template <typename... Ts>
    struct overloaded : Ts... {
        using Ts::operator()...;
    };
    template <typename... Ts>
    overloaded(Ts...) -> overloaded<Ts...>;

    // overloaded factory function
    template <typename... Ts>
    auto make_overloaded(Ts... callables) {
        return overloaded{std::forward<Ts>(callables)...};
    }

    // Convenience function that creates overloaded instance of input callables, and invokes
    // std::visit(overloads, variant)
    template <typename VariantT, typename... Ts>
    auto visit_overloads(VariantT&& variant, Ts&&... callables) {
        return std::visit(make_overloaded(std::forward<Ts>(callables)...),
                          std::forward<VariantT>(variant));
    }

} // namespace std_util
