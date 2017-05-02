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

inline static void init_factory_no_nullptr(void *ptr) {
    if (!ptr) throw type_error("pybind11::init(): factory function returned nullptr");
}

template <typename CFunc, typename CReturn, typename AFuncIn, typename AReturn, typename... Args> struct init_factory {
private:
    using CFuncType = typename std::remove_reference<CFunc>::type;
    static constexpr bool have_alias_factory = !std::is_void<AFuncIn>::value;
    using AFunc = conditional_t<have_alias_factory, AFuncIn, void_type>;
    using AFuncType = typename std::remove_reference<AFunc>::type;

    template <typename Class> using Cpp = typename Class::type;
    template <typename Class> using Alias = typename Class::type_alias;
    template <typename Class> using Inst = typename Class::instance_type;
    template <typename Class> using Holder = typename Class::holder_type;
    template <typename Class> using alias_constructible_from_cpp = std::is_constructible<Alias<Class>, Cpp<Class> &&>;

public:
    // Constructor with a single function/lambda to call
    init_factory(CFunc &&f) : class_factory(std::forward<CFunc>(f)) {}

    // Constructor with two functions/lambdas, for a class with distinct class/alias factories: the
    // first is called when an alias is not needed, the second when the alias is needed.  Requires
    // non-void AFunc.
    init_factory(CFunc &&c, AFunc &&a) :
        class_factory(std::forward<CFunc>(c)), alias_factory(std::forward<AFunc>(a)) {}

    // Add __init__ definition for a class that has no alias or has no separate alias factory
    template <typename Class, typename... Extra,
              enable_if_t<!Class::has_alias || !have_alias_factory, int> = 0>
    void execute(Class &cl, const Extra&... extra) && {
        auto *cl_type = (PyTypeObject *) cl.ptr();
        #if defined(PYBIND11_CPP14) || defined(_MSC_VER)
        cl.def("__init__", [cl_type, func = std::move(class_factory)]
        #else
        CFuncType func(std::move(class_factory));
        cl.def("__init__", [cl_type, func]
        #endif
        (handle self, Args... args) {
            auto *inst = (Inst<Class> *) self.ptr();
            construct<Class>(inst, func(std::forward<Args>(args)...), cl_type);
        }, extra...);
    }

    // Add __init__ definition for a class with an alias *and* distinct alias factory:
    template <typename Class, typename... Extra,
              enable_if_t<Class::has_alias && have_alias_factory, int> = 0>
    void execute(Class &cl, const Extra&... extra) && {
        auto *cl_type = (PyTypeObject *) cl.ptr();
        #if defined(PYBIND11_CPP14) || defined(_MSC_VER)
        cl.def("__init__", [cl_type, class_func = std::move(class_factory), alias_func = std::move(alias_factory)]
        #else
        CFuncType class_func(std::move(class_factory));
        AFuncType alias_func(std::move(alias_factory));
        cl.def("__init__", [cl_type, class_func, alias_func]
        #endif
        (handle self, Args... args) {
            auto *inst = (Inst<Class> *) self.ptr();
            if (Py_TYPE(inst) == cl_type)
                construct<Class>(inst, class_func(std::forward<Args>(args)...), cl_type);
            else
                construct<Class>(inst, alias_func(std::forward<Args>(args)...), cl_type);
        }, extra...);
    }

private:
    template <typename Class> static void dealloc(Inst<Class> *self) {
        // Reset/unallocate the existing values
        clear_instance((PyObject *) self);
        self->value = nullptr;
        self->owned = true;
        self->holder_constructed = false;
    }

    // Error-generating fallback
    template <typename Class>
    static void construct(...) {
        static_assert(!std::is_same<Class, Class>::value /* always false */,
                "pybind11::init(): wrapped factory function must return a compatible pointer, "
                "holder, or value");
    }

    // Pointer assignment implementation:
    template <typename Class>
    static void construct_impl(Inst<Class> *self, Cpp<Class> *ptr) {
        dealloc<Class>(self);
        self->value = ptr;
        register_instance(self, get_type_info(typeid(Cpp<Class>)));
    }

    // Takes a T pointer, assumed to be some base class of the alias, and attempts to dynamic_cast
    // it to the alias class; if it succeeds, returns the Cpp pointer (NOT the Alias pointer); if it
    // fails, returns nullptr.
    template <typename Class, typename T, enable_if_t<Class::has_alias, int> = 0>
    static Cpp<Class> *check_alias(T *ptr) {
        if (auto *alias_ptr = dynamic_cast<Alias<Class> *>(ptr))
            return static_cast<Cpp<Class> *>(alias_ptr);
        return nullptr;
    }
    // Failing fallback version for a non-aliased class (always returns nullptr)
    template <typename Class>
    static Cpp<Class> *check_alias(void *) { return nullptr; }

    // Constructs an alias using a `Alias(Cpp &&)` constructor.  Check alias_constructible_from_cpp<Class>::value
    // before invoking.
    template <typename Class, enable_if_t<alias_constructible_from_cpp<Class>::value, int> = 0>
    static void construct_alias_from_cpp(Inst<Class> *self, Cpp<Class> &&base) {
        new (self->value) Alias<Class>(std::move(base));
    }
    // Dummy version that shouldn't be called (but needs to compile)
    template <typename Class, enable_if_t<!alias_constructible_from_cpp<Class>::value, int> = 0>
    static void construct_alias_from_cpp(Inst<Class> *, Cpp<Class> &&) { }

    // Pointer return v1: a factory function that returns a class pointer for a registered class
    // without an alias
    template <typename Class, enable_if_t<!Class::has_alias, int> = 0>
    static void construct(Inst<Class> *self, Cpp<Class> *ptr, PyTypeObject *) {
        init_factory_no_nullptr(ptr);
        construct_impl<Class>(self, ptr);
    }

    // Pointer return v2: a polymorphic base pointer return that needs a dynamic_cast to downcast to
    // the Cpp class, with no alias class involved.
    template <typename Class, typename T, enable_if_t<!Class::has_alias &&
        std::is_base_of<T, Cpp<Class>>::value && !std::is_base_of<Cpp<Class>, T>::value, int> = 0>
    static void construct(Inst<Class> *self, T *base_ptr, PyTypeObject *) {
        init_factory_no_nullptr(base_ptr);
        Cpp<Class> *ptr = dynamic_cast<Cpp<Class> *>(base_ptr);
        if (!ptr) {
            delete base_ptr;
            throw type_error("pybind11::init(): factory function failed: casting base pointer to instance pointer failed");
        }
        construct_impl<Class>(self, ptr);
    }

    // Pointer return v3: a factory that always returns an alias instance ptr (like py::init_alias)
    template <typename Class, enable_if_t<Class::has_alias, int> = 0>
    static void construct(Inst<Class> *self, Alias<Class> *alias_ptr, PyTypeObject *) {
        construct_impl<Class>(self, static_cast<Cpp<Class> *>(alias_ptr));
    }

    // Pointer return v4: returning a cpp class (or a base thereof) for a class with an alias.  If
    // we don't need an alias, we dynamic_cast to the cpp pointer.  Otherwise, we try a dynamic_cast
    // to the alias class.  If that fails but we can construct the alias with an `Alias(Cpp &&)`
    // constructor, we try a dynamic cast to the cpp class and, if successful, invoke the
    // constructor.  If none of the above works, we throw a type error.
    template <typename Class, typename T,
              enable_if_t<Class::has_alias && std::is_base_of<T, Cpp<Class>>::value, int> = 0>
    static void construct(Inst<Class> *self, T *base_ptr, PyTypeObject *cl_type) {
        init_factory_no_nullptr(base_ptr);
        Cpp<Class> *ptr = nullptr;
        if (Py_TYPE(self) != cl_type) {
            // Alias instance needed
            ptr = check_alias<Class>(base_ptr);
            if (!ptr && alias_constructible_from_cpp<Class>::value) {
                ptr = dynamic_cast<Cpp<Class> *>(base_ptr);
                if (ptr) {
                    construct_alias_from_cpp<Class>(self, std::move(*ptr));
                    delete ptr;
                    return;
                }
            }
            if (!ptr) {
                delete base_ptr;
                throw type_error("pybind11::init(): factory function pointer could not be cast or "
                                 "converted to an alias instance");
            }
        }
        if (!ptr) ptr = dynamic_cast<Cpp<Class> *>(base_ptr);
        if (!ptr) {
            delete base_ptr;
            throw type_error("pybind11::init(): factory function base type pointer is not a class instance");
        }
        construct_impl<Class>(self, ptr);
    }

    // Holder return: copy its pointer, and move or copy the returned holder into the new instance's
    // holder.  This also handles types like std::shared_ptr<T> and std::unique_ptr<T> where T is a
    // derived type (through those holder's implicit conversion from derived class holder constructors).
    template <typename Class>
    static void construct(Inst<Class> *self, Holder<Class> holder, PyTypeObject *cl_type) {
        auto *ptr = holder_helper<Holder<Class>>::get(holder);
        // If we need an alias, check that the held pointer is actually an alias instance
        if (Class::has_alias && Py_TYPE(self) != cl_type && !check_alias<Class>(ptr))
            throw type_error("pybind11::init(): construction failed: returned holder-wrapped instance "
                             "is not an alias instance");

        construct_impl<Class>(self, ptr);
        Class::init_holder((PyObject *) self, &holder);
    }

    // std::shared_ptr<T> returned where T is a base of CppClass (or something invalid); we try a
    // std::dynamic_pointer_cast.
    template <typename Class, typename T, enable_if_t<!std::is_base_of<Cpp<Class>, T>::value, int> = 0>
    static void construct(Inst<Class> *self, std::shared_ptr<T> holder, PyTypeObject *cl_type) {
        // NB: keep this on one line because some compilers (e.g. gcc) show just the message and the
        // line with the static_assert keyword:
        static_assert(all_of<std::is_base_of<T, Cpp<Class>>, std::is_polymorphic<T>, std::is_same<Holder<Class>, std::shared_ptr<Cpp<Class>>>>::value,
            "pybind11::init(): incompatible factory function std::shared_ptr<T> return type: cannot convert shared_ptr<T> to holder");
        // We have a shared_ptr<T> where T is a base of Cpp, and our holder is shared_ptr<Cpp>; try
        // a dynamic cast, with runtime failure if it fails:
        auto h = (Class::has_alias && Py_TYPE(self) != cl_type)
            ? std::static_pointer_cast<Cpp<Class>>(std::dynamic_pointer_cast<Alias<Class>>(holder))
            : std::dynamic_pointer_cast<Cpp<Class>>(holder);
        if (!h)
            throw type_error("pybind11::init(): factory construction failed: base class shared_ptr is not a derived instance");
        construct<Class>(self, std::move(h), nullptr);
    }

    // return-by-value version 1: returning a cpp class by value when there is no alias
    template <typename Class, enable_if_t<!Class::has_alias, int> = 0>
    static void construct(Inst<Class> *self, Cpp<Class> &&result, PyTypeObject *) {
        static_assert(std::is_move_constructible<Cpp<Class>>::value,
            "pybind11::init() return-by-value factory function requires a movable class");
        new (self->value) Cpp<Class>(std::move(result));
    }

    // return-by-value version 2: returning a cpp class by value when there is an alias: the alias
    // must have an `Alias(Base &&)` constructor so that we can construct the alias from the base
    // when needed.
    template <typename Class, enable_if_t<Class::has_alias, int> = 0>
    static void construct(Inst<Class> *self, Cpp<Class> &&result, PyTypeObject *cl_type) {
        static_assert(std::is_move_constructible<Cpp<Class>>::value,
            "pybind11::init() return-by-value factory function requires a movable class");
        if (Py_TYPE(self) != cl_type) {
            if (alias_constructible_from_cpp<Class>::value)
                construct_alias_from_cpp<Class>(self, std::move(result));
            else
                throw type_error("pybind11::init(): unable to convert returned instance to "
                                 "required alias class: no `Alias(Class &&)` constructor available");
        }
        else
            new (self->value) Cpp<Class>(std::move(result));
    }

    // return-by-value version 3: the alias type itself--always initialize via the alias type (this
    // is the factory equivalent of py::init_alias<...>()).
    template <typename Class>
    static void construct(Inst<Class> *self, Alias<Class> &&result, PyTypeObject *) {
        static_assert(std::is_move_constructible<Alias<Class>>::value,
            "pybind11::init() return-by-alias-value factory function requires a movable alias class");
        new (self->value) Alias<Class>(std::move(result));
    }

    CFuncType class_factory;
    AFuncType alias_factory;
};

// Helper definition to infer the detail::init_factory template type from a callable object
template <typename Func, typename Return, typename... Args>
init_factory<Func, Return, void, void, Args...> init_factory_decltype(Return (*)(Args...));
template <typename Return1, typename Return2, typename... Args1, typename... Args2>
inline constexpr bool init_factory_require_matching_arguments(Return1 (*)(Args1...), Return2 (*)(Args2...)) {
    static_assert(sizeof...(Args1) == sizeof...(Args2),
        "pybind11::init(class_factory, alias_factory): class and alias factories must have identical argument signatures");
    static_assert(all_of<std::is_same<Args1, Args2>...>::value,
        "pybind11::init(class_factory, alias_factory): class and alias factories must have identical argument signatures");
    return true;
}
template <typename CFunc, typename AFunc,
          typename CReturn, typename... CArgs, typename AReturn, typename... AArgs,
          bool = init_factory_require_matching_arguments((CReturn (*)(CArgs...)) nullptr, (AReturn (*)(AArgs...)) nullptr)>
init_factory<CFunc, CReturn, AFunc, AReturn, CArgs...> init_factory_decltype(
    CReturn (*)(CArgs...), AReturn (*)(AArgs...));

template <typename... Func> using init_factory_t = decltype(init_factory_decltype<Func...>(
    (typename detail::remove_class<decltype(&std::remove_reference<Func>::type::operator())>::type *) nullptr...));

NAMESPACE_END(detail)

/// Single-argument factory function constructor wrapper
template <typename Func>
detail::init_factory_t<Func> init(Func &&f) { return {std::forward<Func>(f)}; }

/// Dual-argument factory function: the first function is called when no alias is needed, the second
/// when an alias is needed (i.e. due to python-side inheritance).
template <typename CFunc, typename AFunc>
detail::init_factory_t<CFunc, AFunc> init(CFunc &&c, AFunc &&a) {
    return {std::forward<CFunc>(c), std::forward<AFunc>(a)};
}

NAMESPACE_END(pybind11)

#if defined(_MSC_VER)
#  pragma warning(pop)
#endif
