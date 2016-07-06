/*
    pybind11/implicit.h: Enabler for implicit conversion of pybind11-registered types to arbitrary
    C++ types.  This header is only needed for conversion from pybind11-registered types to
    non-pybind-registered types; support for implicit conversion *to* pybind11-registered types does
    not need this header to be included.

    Copyright (c) 2016 Jason Rhinelander <jason@imaginary.ca>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/
#pragma once
#include "cast.h"

NAMESPACE_BEGIN(pybind11)
NAMESPACE_BEGIN(detail)

// Base class for non-destructible types.  We don't use this, but we need ptr and get() to exist
// (they won't be touched at run-time, because we only allow implicit conversion for destructible
// types).
template <typename T, typename SFINAE = void> struct implicit_caster {
    T *ptr = nullptr;
    template <typename W> W get() {
        throw std::logic_error("pybind11 bug: this should not be called."); }
};

// Destructible types: we also manage the T pointer, destroying it during our destruction.
template <typename T> struct implicit_caster<T, typename std::enable_if<std::is_destructible<T>::value>::type> {
    T *ptr = nullptr;
    ~implicit_caster() {
#if defined(__GNUG__) && !defined(__clang__)
   // Disable GCC polymorphic destructor warning here (but leave it enabled elsewhere): we only set
   // ptr to an actual instance, not a base class pointer, so this warning is a false positive.
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdelete-non-virtual-dtor"
#endif
        if (ptr) delete ptr;
#if defined(__GNUG__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
    }
    // We only apply implicit conversion when the type is a pointer or lvalue; the other get() is
    // needed for the code to compile with non-convertible types, so won't actually be called.
    template <typename W> typename std::enable_if<std::is_same<typename intrinsic_type<W>::type, T>::value && std::is_pointer<W>::value, W>::type get() { return ptr; }
    template <typename W> typename std::enable_if<std::is_same<typename intrinsic_type<W>::type, T>::value && std::is_lvalue_reference<W>::value, W>::type get() { return *ptr; }
    template <typename W> typename std::enable_if<!std::is_same<typename intrinsic_type<W>::type, T>::value || !(std::is_pointer<W>::value || std::is_lvalue_reference<W>::value), W>::type get() {
        throw std::logic_error("pybind11 bug: this should not be called."); }
};


template <typename T> struct implicit_conversion_enabled<true, T> : public std::integral_constant<bool, true> {
    template <typename... Tuple> using conversion_type = std::tuple<implicit_caster<typename intrinsic_type<Tuple>::type>...>;
};



NAMESPACE_END(detail)
NAMESPACE_END(pybind11)
