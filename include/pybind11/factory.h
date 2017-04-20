/*
    pybind11/factory.h: Helper class for binding C++ factory functions
                        as Python constructors.

    Copyright (c) 2017 Jason Rhinelander <jason@imaginary.ca>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once
#include "pybind11.h"

#if defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable: 4100) // warning C4100: Unreferenced formal parameter
#  pragma warning(disable: 4127) // warning C4127: Conditional expression is constant
#endif

NAMESPACE_BEGIN(pybind11)

template <typename type, typename... options>
template <typename... Args, typename... Extra>
class_<type, options...> &class_<type, options...>::def(detail::init_factory<Args...> &&init, const Extra&... extra) {
    std::move(init).execute(*this, extra...);
    return *this;
}

NAMESPACE_BEGIN(detail)

template <typename Func, typename Return, typename... Args> struct init_factory {
private:
    using FuncType = typename std::remove_reference<Func>::type;
    using PlainReturn = typename std::remove_pointer<Return>::type;
    using ForwardReturn = typename std::add_rvalue_reference<Return>::type;

    template <typename Class> using Cpp = typename Class::type;
    template <typename Class> using Alias = typename Class::type_alias;
    template <typename Class> using Inst = typename Class::instance_type;
    template <typename Class> using Holder = typename Class::holder_type;

    template <typename Class, typename SFINAE = void> struct alias_constructible : std::false_type {};
    template <typename Class> struct alias_constructible<Class, enable_if_t<Class::has_alias &&
            std::is_convertible<Return, Alias<Class>>::value && std::is_constructible<Alias<Class>, Alias<Class> &&>::value>>
        : std::true_type {};
    template <typename Class> using cpp_constructible = bool_constant<
        !(Class::has_alias && std::is_base_of<Alias<Class>, Return>::value) &&
        std::is_convertible<Return, Cpp<Class>>::value && std::is_constructible<Cpp<Class>, Cpp<Class> &&>::value>;

    template <typename Class, typename = Holder<Class>, typename = Return> struct is_shared_base : std::false_type {};
    template <typename Class, typename R>
    struct is_shared_base<Class, std::shared_ptr<Cpp<Class>>, std::shared_ptr<R>> : std::is_base_of<R, Cpp<Class>> {};

    // We accept a return value in the following categories, in order of precedence:
    struct wraps_pointer_tag {};
    struct wraps_holder_tag {};
    struct wraps_base_shared_ptr_tag {};
    struct wraps_pyobject_tag {};
    struct wraps_value_tag {};
    struct invalid_factory_return_type {};

    // Resolve the combination of Class and Return value to exactly one of the above tags:
    template <typename Class> using factory_type =
        // a pointer of the actual type, a derived type, or a base type:
        conditional_t<all_of<std::is_pointer<Return>, any_of<
                std::is_base_of<Cpp<Class>, PlainReturn>, std::is_base_of<PlainReturn, Cpp<Class>>>>::value,
            wraps_pointer_tag,
        // a holder (including upcasting if supported by the holder (e.g. shared_ptr and unique_ptr))
        conditional_t<std::is_convertible<Return, Holder<Class>>::value,
            wraps_holder_tag,
        // shared_ptr to a base or derived type (only accepted if this type's holder is also shared_ptr)
        conditional_t<is_shared_base<Class>::value,
            wraps_base_shared_ptr_tag,
        // a python object (with compatible type checking and failure at runtime):
        conditional_t<std::is_convertible<Return, handle>::value,
            wraps_pyobject_tag,
        // Accept-by-value: a return convertible to the cpp type and/or the alias:
        conditional_t<alias_constructible<Class>::value || cpp_constructible<Class>::value,
            wraps_value_tag,
        invalid_factory_return_type>>>>>;

public:
    // Constructor: takes the function/lambda to call
    init_factory(Func &&f) : f(std::forward<Func>(f)) {}

    template <typename Class, typename... Extra>
    void execute(Class &cl, const Extra&... extra) && {
        // Some checks against various types of failure that we can detect at compile time:
        static_assert(!std::is_same<factory_type<Class>, invalid_factory_return_type>::value,
                "pybind11::init_factory(): wrapped factory function must return a compatible pointer, "
                "holder, python object, or value");

        PyTypeObject *cl_type = (PyTypeObject *) cl.ptr();
        #if defined(PYBIND11_CPP14) || defined(_MSC_VER)
        cl.def("__init__", [cl_type, func = std::move(f)]
        #else
        FuncType func(std::move(f));
        cl.def("__init__", [cl_type, func]
        #endif
        (handle self, Args... args) {
            auto *inst = (Inst<Class> *) self.ptr();
            construct<Class>(inst, func(std::forward<Args>(args)...), cl_type, factory_type<Class>());
        }, extra...);
    }

protected:
    template <typename Class> static void dealloc(Inst<Class> *self) {
        // Reset/unallocate the existing values
        clear_instance((PyObject *) self);
        self->value = nullptr;
        self->owned = true;
        self->holder_constructed = false;
    }

    template <typename Class>
    static void construct(Inst<Class> *self, PlainReturn *result, PyTypeObject *, wraps_pointer_tag) {
        // We were given a pointer to CppClass (or some derived or base type).  If a base type, try
        // a dynamic_cast to the Cpp; for derived types do a static_cast.  We then dealloc the
        // existing value and replace it with the given pointer; the dispatcher will then set up the
        // holder for us after we return from the lambda.

        constexpr bool downcast = std::is_base_of<PlainReturn, Cpp<Class>>::value &&
                                 !std::is_base_of<Cpp<Class>, PlainReturn>::value;

        if (!result) throw type_error("__init__() factory function returned a null pointer");
        Cpp<Class> *ptr;
        if (downcast) {
            ptr = dynamic_cast<Cpp<Class> *>(result);
            if (!ptr) {
                delete result;
                throw type_error("__init__() factory failed: could not cast base class pointer");
            }
        }
        else {
            ptr = static_cast<Cpp<Class> *>(result);
        }

        dealloc<Class>(self);
        self->value = ptr;
        register_instance(self, get_type_info(typeid(Cpp<Class>)));
    }

    template <typename Class>
    static void construct(Inst<Class> *self, Holder<Class> holder, PyTypeObject *, wraps_holder_tag) {
        // We were returned a holder; copy its pointer, and move/copy the holder into place.
        dealloc<Class>(self);
        self->value = holder_helper<Holder<Class>>::get(holder);
        Class::init_holder((PyObject *) self, &holder);
        register_instance(self, get_type_info(typeid(Cpp<Class>)));
    }

    template <typename Class, typename T>
    static void construct(Inst<Class> *self, std::shared_ptr<T> holder, PyTypeObject *cl_type, wraps_base_shared_ptr_tag) {
        // We have a shared_ptr<T> where T is a base of Cpp, and our holder is shared_ptr<Cpp>
        Holder<Class> h = std::dynamic_pointer_cast<Cpp<Class>>(holder);
        if (!h)
            throw type_error("__init__() factory failed: could not cast shared base class pointer");
        construct<Class>(self, std::move(h), cl_type, wraps_holder_tag());
    }

    template <typename Class>
    static void construct(Inst<Class> *self, handle result, PyTypeObject *cl_type, wraps_pyobject_tag tag) {
        // We were given a raw handle; steal it and forward to the py::object version
        construct<Class>(self, reinterpret_steal<object>(result), cl_type, tag);
    }
    template <typename Class>
    static void construct(Inst<Class> *self, object result, PyTypeObject *, wraps_pyobject_tag) {
        // Lambda returned a py::object (or something derived from it)

        // Make sure we actually got something
        if (!result)
            throw type_error("__init__() factory function returned a null python object");

        auto *result_inst = (Inst<Class> *) result.ptr();
        auto type = Py_TYPE(self);

        // Make sure the factory function gave us exactly the right type (we don't allow
        // up/down-casting here):
        if (Py_TYPE(result_inst) != type)
            throw type_error(std::string("__init__() factory function should return '") + type->tp_name +
                "', not '" + Py_TYPE(result_inst)->tp_name + "'");
        // The factory function must give back a unique reference:
        if (result.ref_count() != 1)
            throw type_error("__init__() factory function returned an object with multiple references");
        // Guard against accidentally specifying a reference r.v. policy or similar:
        if (!result_inst->owned)
            throw type_error("__init__() factory function returned an unowned reference");

        // Steal the instance internals:
        dealloc<Class>(self);
        std::swap(self->value, result_inst->value);
        std::swap(self->weakrefs, result_inst->weakrefs);
        if (type->tp_dictoffset != 0)
            std::swap(*_PyObject_GetDictPtr((PyObject *) self), *_PyObject_GetDictPtr((PyObject *) result_inst));
        // Now steal the holder
        Class::init_holder((PyObject *) self, &result_inst->holder);
        // Find the instance we just stole and update its PyObject from `result` to `self`
        auto range = get_internals().registered_instances.equal_range(self->value);
        for (auto it = range.first; it != range.second; ++it) {
            if (type == Py_TYPE(it->second)) {
                it->second = self;
                break;
            }
        }
    }

    // return-by-value version 1: no alias or return not convertible to the alias:
    template <typename Class, enable_if_t<!alias_constructible<Class>::value, int> = 0>
    static void construct(Inst<Class> *self, Return &&result, PyTypeObject *cl_type, wraps_value_tag) {
        // Fail if we require an alias (i.e. if we're inherited from on the Python side)
        if (Class::has_alias && Py_TYPE(self) != cl_type)
            throw type_error("__init__() factory failed: cannot construct required alias class from factory return value");
        construct_cpp<Class>(self, std::forward<Return>(result));
    }

    // return-by-value version 2: the alias type itself or something convertible to it but not (directly) to the cpp type;
    // always initialize via the alias type:
    template <typename Class, enable_if_t<alias_constructible<Class>::value && !cpp_constructible<Class>::value, int> = 0>
    static void construct(Inst<Class> *self, Return &&result, PyTypeObject *, wraps_value_tag) {
        construct_alias<Class>(self, std::forward<Return>(result));
    }

    // return-by-value version 3: the return is convertible to both class and alias; construct via
    // alias if we're being used as a subclass, otherwise construct via cpp class.
    template <typename Class, enable_if_t<alias_constructible<Class>::value && cpp_constructible<Class>::value, int> = 0>
    static void construct(Inst<Class> *self, Return &&result, PyTypeObject *cl_type, wraps_value_tag) {
        // Use calls (rather than constructing directly) to properly trigger implicit conversion
        if (Py_TYPE(self) != cl_type)
            construct_alias<Class>(self, std::forward<Return>(result));
        else
            construct_cpp<Class>(self, std::forward<Return>(result));
    }
    template <typename Class> static void construct_alias(Inst<Class> *self, Alias<Class> result) {
        new (self->value) Alias<Class>(std::move(result));
    }
    template <typename Class> static void construct_cpp(Inst<Class> *self, Cpp<Class> result) {
        new (self->value) Cpp<Class>(std::move(result));
    }

    FuncType f;
};

// Helper definition to infer the detail::init_factory template type from a callable object
template <typename Func, typename Return, typename... Args>
init_factory<Func, Return, Args...> init_factory_decltype(Return (*)(Args...));
template <typename Func> using init_factory_t = decltype(init_factory_decltype<Func>(
    (typename detail::remove_class<decltype(&std::remove_reference<Func>::type::operator())>::type *) nullptr));

NAMESPACE_END(detail)

/// Construct a factory function constructor wrapper from a vanilla function pointer
template <typename Return, typename... Args>
detail::init_factory<Return (*)(Args...), Return, Args...> init_factory(Return (*f)(Args...)) {
    return f;
}
/// Construct a factory function constructor wrapper from a lambda function (possibly with internal state)
template <typename Func, typename = detail::enable_if_t<
    detail::satisfies_none_of<
        typename std::remove_reference<Func>::type,
        std::is_function, std::is_pointer, std::is_member_pointer
    >::value>
>
detail::init_factory_t<Func> init_factory(Func &&f) { return std::forward<Func>(f); }

NAMESPACE_END(pybind11)

#if defined(_MSC_VER)
#  pragma warning(pop)
#endif
