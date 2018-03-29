/*
    pybind11/caster/pytypes.h: type casters for Python type wrappers (py::object, py::dict, etc.)

    Copyright (c) 2018 Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/
#pragma once
#include "base.h"

NAMESPACE_BEGIN(PYBIND11_NAMESPACE)
NAMESPACE_BEGIN(detail)

template <typename T> struct handle_type_name { static constexpr auto name = _<T>(); };
template <> struct handle_type_name<bytes> { static constexpr auto name = _(PYBIND11_BYTES_NAME); };
template <> struct handle_type_name<args> { static constexpr auto name = _("*args"); };
template <> struct handle_type_name<kwargs> { static constexpr auto name = _("**kwargs"); };

template <typename type>
struct pyobject_caster {
    template <typename T = type, enable_if_t<std::is_same<T, handle>::value, int> = 0>
    bool load(handle src, bool /* convert */) { value = src; return static_cast<bool>(value); }

    template <typename T = type, enable_if_t<std::is_base_of<object, T>::value, int> = 0>
    bool load(handle src, bool /* convert */) {
        if (!isinstance<type>(src))
            return false;
        value = reinterpret_borrow<type>(src);
        return true;
    }

    static handle cast(const handle &src, return_value_policy /* policy */, handle /* parent */) {
        return src.inc_ref();
    }
    PYBIND11_TYPE_CASTER(type, handle_type_name<type>::name);
};

template <typename T>
class type_caster<T, enable_if_t<is_pyobject<T>::value>> : public pyobject_caster<T> { };

NAMESPACE_END(detail)
NAMESPACE_END(PYBIND11_NAMESPACE)
