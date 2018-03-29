/*
    pybind11/caster/numeric.h: type casters for primitive numeric types

    Copyright (c) 2016 Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/
#pragma once
#include "base.h"

NAMESPACE_BEGIN(PYBIND11_NAMESPACE)
NAMESPACE_BEGIN(detail)

template <> class type_caster<bool> {
public:
    bool load(handle src, bool convert) {
        if (!src) return false;
        else if (src.ptr() == Py_True) { value = true; return true; }
        else if (src.ptr() == Py_False) { value = false; return true; }
        else if (convert || !strcmp("numpy.bool_", Py_TYPE(src.ptr())->tp_name)) {
            // (allow non-implicit conversion for numpy booleans)

            Py_ssize_t res = -1;
            if (src.is_none()) {
                res = 0;  // None is implicitly converted to False
            }
            #if defined(PYPY_VERSION)
            // On PyPy, check that "__bool__" (or "__nonzero__" on Python 2.7) attr exists
            else if (hasattr(src, PYBIND11_BOOL_ATTR)) {
                res = PyObject_IsTrue(src.ptr());
            }
            #else
            // Alternate approach for CPython: this does the same as the above, but optimized
            // using the CPython API so as to avoid an unneeded attribute lookup.
            else if (auto tp_as_number = src.ptr()->ob_type->tp_as_number) {
                if (PYBIND11_NB_BOOL(tp_as_number)) {
                    res = (*PYBIND11_NB_BOOL(tp_as_number))(src.ptr());
                }
            }
            #endif
            if (res == 0 || res == 1) {
                value = (bool) res;
                return true;
            }
        }
        return false;
    }
    static handle cast(bool src, return_value_policy /* policy */, handle /* parent */) {
        return handle(src ? Py_True : Py_False).inc_ref();
    }
    PYBIND11_TYPE_CASTER(bool, _("bool"));
};

template <typename T>
struct type_caster<T, enable_if_t<std::is_arithmetic<T>::value && !is_std_char_type<T>::value>> {
    using _py_type_0 = conditional_t<sizeof(T) <= sizeof(long), long, long long>;
    using _py_type_1 = conditional_t<std::is_signed<T>::value, _py_type_0, typename std::make_unsigned<_py_type_0>::type>;
    using py_type = conditional_t<std::is_floating_point<T>::value, double, _py_type_1>;
public:

    bool load(handle src, bool convert) {
        py_type py_value;

        if (!src)
            return false;

        if (std::is_floating_point<T>::value) {
            if (convert || PyFloat_Check(src.ptr()))
                py_value = (py_type) PyFloat_AsDouble(src.ptr());
            else
                return false;
        } else if (PyFloat_Check(src.ptr())) {
            return false;
        } else if (std::is_unsigned<py_type>::value) {
            py_value = as_unsigned<py_type>(src.ptr());
        } else { // signed integer:
            py_value = sizeof(T) <= sizeof(long)
                ? (py_type) PyLong_AsLong(src.ptr())
                : (py_type) PYBIND11_LONG_AS_LONGLONG(src.ptr());
        }

        bool py_err = py_value == (py_type) -1 && PyErr_Occurred();
        if (py_err || (std::is_integral<T>::value && sizeof(py_type) != sizeof(T) &&
                       (py_value < (py_type) std::numeric_limits<T>::min() ||
                        py_value > (py_type) std::numeric_limits<T>::max()))) {
            bool type_error = py_err && PyErr_ExceptionMatches(
#if PY_VERSION_HEX < 0x03000000 && !defined(PYPY_VERSION)
                PyExc_SystemError
#else
                PyExc_TypeError
#endif
            );
            PyErr_Clear();
            if (type_error && convert && PyNumber_Check(src.ptr())) {
                auto tmp = reinterpret_steal<object>(std::is_floating_point<T>::value
                                                     ? PyNumber_Float(src.ptr())
                                                     : PyNumber_Long(src.ptr()));
                PyErr_Clear();
                return load(tmp, false);
            }
            return false;
        }

        value = (T) py_value;
        return true;
    }

    static handle cast(T src, return_value_policy /* policy */, handle /* parent */) {
        if (std::is_floating_point<T>::value) {
            return PyFloat_FromDouble((double) src);
        } else if (sizeof(T) <= sizeof(ssize_t)) {
            // This returns a long automatically if needed
            if (std::is_signed<T>::value)
                return PYBIND11_LONG_FROM_SIGNED(src);
            else
                return PYBIND11_LONG_FROM_UNSIGNED(src);
        } else {
            if (std::is_signed<T>::value)
                return PyLong_FromLongLong((long long) src);
            else
                return PyLong_FromUnsignedLongLong((unsigned long long) src);
        }
    }

    PYBIND11_TYPE_CASTER(T, _<std::is_integral<T>::value>("int", "float"));
};

NAMESPACE_END(detail)
NAMESPACE_END(PYBIND11_NAMESPACE)
