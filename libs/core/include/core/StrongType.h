#pragma once

// Creates a strong/named type for a common type T.
//
// Example:
//
// using Width = StrongType<int, struct WidthType>;
// using Height = StrongType<int, struct HeightType>;
//
// Inspired by https://www.fluentcpp.com/2016/12/08/strong-types-for-strong-interfaces/
//
template <typename T, typename ParameterType>
struct StrongType {
    StrongType() = default;

    explicit StrongType(T value)
        : m_value(std::move(value)) {}

    // const T& Value() const { return m_value; }
    // T& Value() { return m_value; }

    operator T&() { return m_value; }
    operator const T &() const { return m_value; }

    // TODO: Because we allow implicit conversion to T, implement all comparison operators available
    // for T so that StrongTypes of different Ts cannot compare. Alternatively, don't allow implicit
    // conversion and force user to cast or user a getter for the wrapped value.

private:
    T m_value;
};
