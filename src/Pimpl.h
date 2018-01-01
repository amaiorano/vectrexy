#pragma once

#include <cassert>
#include <type_traits>

// Pimpl (Private IMPLemenation) is a class used to declare storage for an undefined type T, while
// providing a way to construct and then access T's members through the type. It's similar to using
// std::unique_ptr except that it avoids heap allocation and access.
//
// T should be a forward-declared type for this class to be useful. If Pimpl<T> is a data member
// of Foo, then you'll need to define Foo's destructor in the translation unit where T is
// defined. You must call Construct() once on the Pimpl instance, and can access members of T
// using Pimpl's pointer-like interface.

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

Foo() {
    m_bar.Construct();
}

Foo::~Foo() = default;

*/

namespace pimpl {
    enum class SizePolicy {
        Exact,  // Size == sizeof(T)
        AtLeast // Size >= sizeof(T)
    };

    template <typename T, size_t Size, SizePolicy SizePolicy = SizePolicy::AtLeast>
    class Pimpl {
        constexpr void ValidateSize() {
            if constexpr (SizePolicy == SizePolicy::AtLeast) {
                static_assert(Size >= sizeof(T), "Pimpl sizeof(T) must be at least 'Size'");
            } else if constexpr (SizePolicy == SizePolicy::Exact) {
                static_assert(Size == sizeof(T), "Pimpl sizeof(T) must be exactly 'Size'");
            } else {
                static_assert(false);
            }
        }

    public:
        ~Pimpl() {
            ValidateSize();

            if (m_value) {
                Destruct();
            }
        }

        // Default constructor does not construct T into storage; Construct must be invoked.
        Pimpl() = default;

        template <typename... Args>
        void Construct(Args&&... args) {
            assert(!m_constructed);
            new (&m_storage) T(std::forward<Args>(args)...);
            m_value = reinterpret_cast<T*>(&m_storage);
        }

        T* Value() { m_value; }
        const T* Value() const { m_value; }

        T* operator->() { return m_value; }
        const T* operator->() const { return m_value; }

        T& operator*() { return *m_value; }
        const T& operator*() const { return *m_value; }

    private:
        void Destruct() {
            assert(m_constructed);
            // NOTE: If you get a compiler error about "use of undefined type" here, it's likely
            // because you need to define a destructor for the owning type in the cpp file. See
            // usage notes at the top of this file.
            reinterpret_cast<T*>(&m_storage)->~T();
            m_value = nullptr;
        }

        std::aligned_storage_t<Size> m_storage;
        T* m_value = nullptr;
    };

} // namespace pimpl
