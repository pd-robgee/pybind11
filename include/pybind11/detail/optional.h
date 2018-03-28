/*
    pybind11/detail/optional.h -- std::optional partial work-alike for C++11/14

    Copyright (c) 2018 Jason Rhinelander <jason@imaginary.ca>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/
#pragma once
#include "common.h"

NAMESPACE_BEGIN(PYBIND11_NAMESPACE)
NAMESPACE_BEGIN(detail)

// If we are in C++17 mode, just use std::optional.
#ifdef __has_include
// std::optional (but including it in c++14 mode isn't allowed)
#  if defined(PYBIND11_CPP17) && __has_include(<optional>)
#    include <optional>
#    define PYBIND11_HAS_OPTIONAL 1
#  endif
// std::experimental::optional (but not allowed in c++11 mode)
#  if defined(PYBIND11_CPP14) && (__has_include(<experimental/optional>) && \
                                 !__has_include(<optional>))
#    include <experimental/optional>
#    define PYBIND11_HAS_EXP_OPTIONAL 1
#  endif
#elif defined(_MSC_VER) && defined(PYBIND11_CPP17)
#  include <optional>
#  define PYBIND11_HAS_OPTIONAL 1
#endif

/** This class supports a subset of `std::optional`: basically enough for what we need for storing
 * instances in type casters.  When compiling in C++17 mode, this is simply an alias for
 * `std::optional`; in C++14, this might be an alias for std::experimental::optional<T>; otherwise
 * we provide our own limited implementation.
 */

#ifdef PYBIND11_HAS_OPTIONAL
template <typename T> using optional = std::optional<T>;
#elif defined(PYBIND11_HAS_EXP_OPTIONAL)
template <typename T> using optional = std::experimental::optional<T>;
#else
#include <cstdint>

template <typename T> class optional {
private:
    alignas(alignof(T)) std::array<std::uint8_t, sizeof(T)> value;
    T *ptr() { return reinterpret_cast<T *>(value.data()); }
    const T *ptr() const { return reinterpret_cast<const T *>(value.data()); }
    bool set = false;
public:
    using value_type = T;
    constexpr optional() = default;
    constexpr optional(optional &&other) {
        new (ptr()) T(std::move(*other));
        set = true;
    }
    template <typename U = T, enable_if_t<std::is_constructible<T, U &&>, int> = 0>
    optional(U &&val) {
        new (ptr()) T(std::move(val));
        set = true;
    }
    optional &operator=(const optional &other) {
        if (other) {
            if (set) {
                *ptr() = *other;
            } else {
                new (ptr()) T(*other);
                set = true;
            }
        } else {
            clear();
        }
        return *this;
    }
    optional &operator=(optional &&other) {
        if (other) {
            if (set) {
                *ptr() = *std::move(other);
            } else {
                new (ptr()) T(*std::move(other));
                set = true;
            }
        } else {
            clear();
        }
        return *this;
    }

    void reset() {
        if (set) {
            ptr()->T::~T();
            set = false;
        }
    }

    constexpr bool has_value() const noexcept { return set; }
    constexpr explicit operator bool() const noexcept { return set; }

    constexpr const T *operator->() const { return ptr(); }
    constexpr T *operator->() { return ptr(); }
    constexpr const T &operator*() const & { return *ptr(); }
    constexpr T &operator*() & { return *ptr(); }
    constexpr const T &&operator*() const && { return std::move(*ptr()); }
    constexpr T &&operator*() && { return std::move(*ptr()); }

    ~optional() { if (set) ptr()->T::~T(); }
};

#endif
