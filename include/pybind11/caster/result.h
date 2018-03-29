/*
    pybind11/caster/result.h: class for holding the result of a type caster conversion

    Copyright (c) 2018 Jason Rhinelander <jason@imaginary.ca>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/
#pragma once
#include "../detail/common.h"
#include <cstdint>

NAMESPACE_BEGIN(PYBIND11_NAMESPACE)
NAMESPACE_BEGIN(caster)

/**
 * caster::result<T, Policy> is the class returned by new-style type casters.  It contains an
 * optional value, a flag indicating whether the value is set, and a flag indicating that an error
 * occurred.  The error message and the value overlap: that is, a caster::result can contain either a
 * value or an error message but not both.  The error message can be used to provide an error reason
 * for a cast failure.
 */
template <typename T>
struct result {
private:
    constexpr static size_t max(size_t a, size_t b) { return a > b ? a : b; }

    alignas(max(alignof(T), alignof(std::string))) std::array<std::uint8_t, max(sizeof(T), sizeof(std::string))> value;
    bool set = false, err = false;

    T *ptr() { return reinterpret_cast<T *>(value.data()); }
    const T *ptr() const { return reinterpret_cast<const T *>(value.data()); }
    std::string *str() { return reinterpret_cast<std::string *>(value.data()); }
    const std::string *str() const { return reinterpret_cast<const std::string *>(value.data()); }
public:
    using value_type = T;
    /// Default constructor: the value and error message are unset.
    constexpr result() = default;

    /// Move constructor; value gets move-constructed from `other`'s value, if set.
    constexpr result(result &&other) : set{other.set}, err{other.err} {
        if (set) new (ptr()) T(*std::move(other));
        else if (err) new (str()) std::string(std::move(other).error());
    }

    /// Constructs a value from some U rvalue.  Requires that T is constructible from such a U &&.
    template <typename U = T, detail::enable_if_t<std::is_constructible<T, U &&>::value, int> = 0>
    result(U &&val) : set{true} {
        new (ptr()) T(std::forward<U>(val));
    }

    /// Copy assignment; the value or the error message are copied from `other`.  If assigning from
    /// `other` with the same value/error string state, the value/error string is copy-assigned.
    /// Otherwise destruction on the error/value is performed, if necessary, and the value/error
    /// string is copy-constructed from `other`'s value/error string.  This operator only
    /// participates if `T` is both copy constructible and assignable.
    template <detail::enable_if_t<detail::is_copy_constructible<T>::value && std::is_copy_constructible<T>::value, int> = 0>
    result &operator=(const result &other) {
        if ((set && !other.set) || (err && !other.err))
            reset();

        if (set)
            *ptr() = *other;
        else if (err)
            *str() = other.error();
        else if (other.set) {
            new (ptr()) T(*other);
            set = true;
        } else if (other.err) {
            new (str()) std::string(other.error());
            err = true;
        }

        return *this;
    }

    /// Move assignment; move-assigns or move-constructs the value or error from the value of
    /// `error()` of `other`.  Destruction on the current value or error is performed if switching
    /// from a value to an error or vice versa, or if `other` has neither value nor error.  This
    /// operator only participates if `T` is both move constructible and assignable.
    template <detail::enable_if_t<std::is_move_constructible<T>::value && std::is_move_constructible<T>::value, int> = 0>
    result &operator=(result &&other) {
        if ((set && !other.set) || (err && !other.err))
            reset();

        if (set)
            *ptr() = *std::move(other);
        else if (err)
            *str() = std::move(other).error();
        else if (other.set) {
            new (ptr()) T(*std::move(other));
            set = true;
        } else if (other.err) {
            new (str()) std::string(std::move(other).error());
            err = true;
        }

        return *this;
    }

    /// Value move assignment.  Moves the value into this object's value, by move assignment if this
    /// object is already set, otherwise by move construction.  If an error is currently set, it is
    /// first unset.
    template <typename U = T, detail::enable_if_t<std::is_constructible<T, U>::value && std::is_assignable<T &, U>::value, int> = 0>
    result &operator=(U &&value) {
        if (err)
            reset();
        if (set)
            *ptr() = std::forward<U>(value);
        else
            new (ptr()) T(std::forward<U>(value));
    }

    /// Destroys the currently value or error message, if set.
    void reset() {
        if (set) {
            ptr()->T::~T();
            set = false;
        } else if (err) {
            str()->std::string::~string();
            err = false;
        }
    }

    /// Returns true if the value is currently set
    constexpr bool has_value() const noexcept { return set; }
    /// Returns true if the value is currently set
    constexpr explicit operator bool() const noexcept { return set; }

    /// Returns the currently stored value.  This does *not* check that the result actually has a
    /// value: the caller must check `has_value()` or `operator bool` before invoking these
    /// operators.
    constexpr const T *operator->() const { return ptr(); }
    constexpr T *operator->() { return ptr(); }
    constexpr const T &operator*() const & { return *ptr(); }
    constexpr T &operator*() & { return *ptr(); }
    constexpr const T &&operator*() const && { return std::move(*ptr()); }
    constexpr T &&operator*() && { return std::move(*ptr()); }

    /// Returns true if this object currently stores an error string.
    constexpr bool has_error() const noexcept { return err; }

    /// Returns the currently stored error message.  This does *not* check that the current result
    /// actually is an error message: the caller must check `has_error()` before invoking these
    /// methods.
    const std::string &error() const & { return *str(); }
    std::string &&error() && { return std::move(*str()); }

    /// Destructor: resets the value or error message (if currently set)
    ~result() { reset(); }
};

NAMESPACE_END(caster)
NAMESPACE_END(PYBIND11_NAMESPACE)
