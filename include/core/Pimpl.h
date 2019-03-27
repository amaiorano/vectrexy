#pragma once

#include <cassert>
#include <cstddef>
#include <type_traits>
#include <utility>

// Define to 0 to remove extra m_value member to Pimpl which is useful for being able to see the T*
// value at debug-time at the expense of increasing Pimpl's size by an extra pointer + potential
// alignment padding.
#if !defined(PIMPL_ADD_VALUE_MEMBER)
#define PIMPL_ADD_VALUE_MEMBER 1
#endif

// Pimpl (Private IMPLemenation) is a class used to declare storage for an undefined type T, while
// providing a way to construct and then access T's members through the type. It's similar to using
// std::unique_ptr except that it avoids heap allocation and access.
//
// T should be a forward-declared type for this class to be useful. If Pimpl<T> is a data member
// of Foo, then you'll need to define Foo's constructor and destructor in the translation unit where
// T is defined. You can access members of T using Pimpl's pointer-like interface.

/* Typical usage:

// Foo.h:
class Foo {
public:
    Foo();
    ~Foo();
private:
    pimpl::Pimpl<class Bar> m_bar;
};

// Foo.cpp:
#include "Bar.h"

Foo::Foo() = default;
Foo::~Foo() = default;

*/

namespace pimpl {
    enum class SizePolicy {
        Exact,  // Size == sizeof(T)
        AtLeast // Size >= sizeof(T)
    };

    template <typename T, size_t Size, SizePolicy SizePolicy = SizePolicy::AtLeast>
    class Pimpl {

        // Required wrapper for if constexpr
        template <class U>
        struct dependent_false : std::false_type {};

        constexpr void ValidateSize() {
            if constexpr (SizePolicy == SizePolicy::AtLeast) {
                static_assert(Size >= sizeof(T), "Pimpl sizeof(T) must be at least 'Size'");
            } else if constexpr (SizePolicy == SizePolicy::Exact) {
                static_assert(Size == sizeof(T), "Pimpl sizeof(T) must be exactly 'Size'");
            } else {
                static_assert(dependent_false<T>::value);
            }
        }

    public:
        // Default constructor constructs T into storage, so T must be defined
        template <typename... Args>
        Pimpl(Args&&... args) {
            SetValue();
            Construct(std::forward<Args>(args)...);
        }

        // Destructor invokes T's destructor, so T must be defined
        ~Pimpl() {
            ValidateSize();
            Destruct();
        }

        // Copy
        Pimpl(const Pimpl& rhs) {
            SetValue();
            CopyAssign(rhs);
        }

        // Copy assign
        Pimpl& operator=(const Pimpl& rhs) {
            if (this != &rhs) {
                CopyAssign(rhs);
            }
            return *this;
        }

        // Move
        Pimpl(Pimpl&& rhs) {
            SetValue();
            MoveAssign(std::move(rhs));
        }

        // Move assign
        Pimpl& operator=(Pimpl&& rhs) {
            if (this != &rhs) {
                MoveAssign(std::move(rhs));
            }
            return *this;
        }

        // Accessors

        T* Value() { return reinterpret_cast<T*>(&m_storage); }
        const T* Value() const { return reinterpret_cast<const T*>(&m_storage); }

        T* operator->() { return Value(); }
        const T* operator->() const { return Value(); }

        T& operator*() { return *Value(); }
        const T& operator*() const { return *Value(); }

    private:
        template <typename... Args>
        void Construct(Args&&... args) {
            new (&m_storage) T(std::forward<Args>(args)...);
        }

        void CopyAssign(const Pimpl& rhs) { new (&m_storage) T(*rhs.Value()); }

        void MoveAssign(Pimpl&& rhs) { new (&m_storage) T(std::move(*rhs.Value())); }

        void Destruct() {
            // NOTE: If you get a compiler error about "use of undefined type" here, it's likely
            // because you need to define both constructor and destructor for the owning type in the
            // cpp file. See usage notes at the top of this file.
            Value()->~T();
        }

        void SetValue() {
#if PIMPL_ADD_VALUE_MEMBER
            m_value = Value();
#endif
        }

#if PIMPL_ADD_VALUE_MEMBER
        T* m_value = nullptr; // Not necessary but useful for debugging
#endif
        std::aligned_storage_t<Size> m_storage;
    };

} // namespace pimpl
