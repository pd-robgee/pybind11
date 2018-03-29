/*
    pybind11/caster/compat.h: backwards-compatibility layer for old-style type casters

    Copyright (c) 2018 Jason Rhinelander <jason@imaginary.ca>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/
#pragma once
#include "base.h"

// Old-style type casters lived in the pybind11::detail namespace as specializations of
// detail::type_caster<T, SFINAE>.  This header provides compatibility functions for converting
// old-style type casters into the new-style type caster interface.

NAMESPACE_BEGIN(PYBIND11_NAMESPACE)
NAMESPACE_BEGIN(detail)

struct no_custom_type_caster {};

template <typename T, typename SFINAE>
struct type_caster : no_custom_type_caster {};

template <typename T> using make_caster = caster::make<T>;

// Shortcut for calling a caster's `cast_op_type` cast operator for casting a type_caster to a T
template <typename T> typename make_caster<T>::template cast_op_type<T> cast_op(make_caster<T> &caster) {
    return caster.operator typename make_caster<T>::template cast_op_type<T>();
}
template <typename T> typename make_caster<T>::template cast_op_type<typename std::add_rvalue_reference<T>::type>
cast_op(make_caster<T> &&caster) {
    return std::move(caster).operator
        typename make_caster<T>::template cast_op_type<typename std::add_rvalue_reference<T>::type>();
}

#define PYBIND11_TYPE_CASTER(type, py_name) \
    protected: \
        type value; \
    public: \
        static constexpr auto name = py_name; \
        template <typename T_, enable_if_t<std::is_same<type, remove_cv_t<T_>>::value, int> = 0> \
        static handle cast(T_ *src, return_value_policy policy, handle parent) { \
            if (!src) return none().release(); \
            if (policy == return_value_policy::take_ownership) { \
                auto h = cast(std::move(*src), policy, parent); delete src; return h; \
            } else { \
                return cast(*src, policy, parent); \
            } \
        } \
        operator type*() { return &value; } \
        operator type&() { return value; } \
        operator type&&() && { return std::move(value); } \
        template <typename T_> using cast_op_type = pybind11::detail::movable_cast_op_type<T_>

NAMESPACE_END(detail)
NAMESPACE_END(PYBIND11_NAMESPACE)
