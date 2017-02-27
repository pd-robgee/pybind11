/*
    pybind11/class_support.h: Python C API implementation details for py::class_

    Copyright (c) 2017 Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once
#include <iostream>

#include "attr.h"

NAMESPACE_BEGIN(pybind11)
NAMESPACE_BEGIN(detail)

#if !defined(PYPY_VERSION)

/// `pybind11_static_property.__get__()`: Always pass the class instead of the instance.
extern "C" inline PyObject *pybind11_static_get(PyObject *self, PyObject * /*ob*/, PyObject *cls) {
    return PyProperty_Type.tp_descr_get(self, cls, cls);
}

/// `pybind11_static_property.__set__()`: Just like the above `__get__()`.
extern "C" inline int pybind11_static_set(PyObject *self, PyObject *obj, PyObject *value) {
    PyObject *cls = PyType_Check(obj) ? obj : (PyObject *) Py_TYPE(obj);
    return PyProperty_Type.tp_descr_set(self, cls, value);
}

/** A `static_property` is the same as a `property` but the `__get__()` and `__set__()`
    methods are modified to always use the object type instead of a concrete instance.
    Return value: New reference. */
inline PyTypeObject *make_static_property_type() {
    constexpr auto *name = "pybind11_static_property";
    auto name_obj = reinterpret_steal<object>(PYBIND11_FROM_STRING(name));

    /* Danger zone: from now (and until PyType_Ready), make sure to
       issue no Python C API calls which could potentially invoke the
       garbage collector (the GC will call type_traverse(), which will in
       turn find the newly constructed type in an invalid state) */
    auto heap_type = (PyHeapTypeObject *) PyType_Type.tp_alloc(&PyType_Type, 0);
    if (!heap_type)
        pybind11_fail("make_static_property_type(): error allocating type!");

    heap_type->ht_name = name_obj.inc_ref().ptr();
#if PY_MAJOR_VERSION >= 3 && PY_MINOR_VERSION >= 3
    heap_type->ht_qualname = name_obj.inc_ref().ptr();
#endif

    auto type = &heap_type->ht_type;
    type->tp_name = name;
    type->tp_base = &PyProperty_Type;
    type->tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HEAPTYPE;
    type->tp_descr_get = pybind11_static_get;
    type->tp_descr_set = pybind11_static_set;

    if (PyType_Ready(type) < 0)
        pybind11_fail("make_static_property_type(): failure in PyType_Ready()!");

    return type;
}

#else // PYPY

/** PyPy has some issues with the above C API, so we evaluate Python code instead.
    This function will only be called once so performance isn't really a concern.
    Return value: New reference. */
inline PyTypeObject *make_static_property_type() {
    auto d = dict();
    PyObject *result = PyRun_String(R"(\
        class pybind11_static_property(property):
            def __get__(self, obj, cls):
                return property.__get__(self, cls, cls)

            def __set__(self, obj, value):
                cls = obj if isinstance(obj, type) else type(obj)
                property.__set__(self, cls, value)
        )", Py_file_input, d.ptr(), d.ptr()
    );
    if (result == nullptr)
        throw error_already_set();
    Py_DECREF(result);
    return (PyTypeObject *) d["pybind11_static_property"].cast<object>().release().ptr();
}

#endif // PYPY

/** Types with static properties need to handle `Type.static_prop = x` in a specific way.
    By default, Python replaces the `static_property` itself, but for wrapped C++ types
    we need to call `static_property.__set__()` in order to propagate the new value to
    the underlying C++ data structure. */
extern "C" inline int pybind11_meta_setattro(PyObject* obj, PyObject* name, PyObject* value) {
    // Use `_PyType_Lookup()` instead of `PyObject_GetAttr()` in order to get the raw
    // descriptor (`property`) instead of calling `tp_descr_get` (`property.__get__()`).
    PyObject *descr = _PyType_Lookup((PyTypeObject *) obj, name);

    // Call `static_property.__set__()` instead of replacing the `static_property`.
    if (descr && PyObject_IsInstance(descr, (PyObject *) get_internals().static_property_type)) {
#if !defined(PYPY_VERSION)
        return Py_TYPE(descr)->tp_descr_set(descr, obj, value);
#else
        if (PyObject *result = PyObject_CallMethod(descr, "__set__", "OO", obj, value)) {
            Py_DECREF(result);
            return 0;
        } else {
            return -1;
        }
#endif
    } else {
        return PyType_Type.tp_setattro(obj, name, value);
    }
}

/** This metaclass is assigned by default to all pybind11 types and is required in order
    for static properties to function correctly. Users may override this using `py::metaclass`.
    Return value: New reference. */
inline PyTypeObject* make_default_metaclass() {
    constexpr auto *name = "pybind11_type";
    auto name_obj = reinterpret_steal<object>(PYBIND11_FROM_STRING(name));

    /* Danger zone: from now (and until PyType_Ready), make sure to
       issue no Python C API calls which could potentially invoke the
       garbage collector (the GC will call type_traverse(), which will in
       turn find the newly constructed type in an invalid state) */
    auto heap_type = (PyHeapTypeObject *) PyType_Type.tp_alloc(&PyType_Type, 0);
    if (!heap_type)
        pybind11_fail("make_default_metaclass(): error allocating metaclass!");

    heap_type->ht_name = name_obj.inc_ref().ptr();
#if PY_MAJOR_VERSION >= 3 && PY_MINOR_VERSION >= 3
    heap_type->ht_qualname = name_obj.inc_ref().ptr();
#endif

    auto type = &heap_type->ht_type;
    type->tp_name = name;
    type->tp_base = &PyType_Type;
    type->tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HEAPTYPE;

//    type->tp_new = pybind11_meta_new;
    type->tp_setattro = pybind11_meta_setattro;

    if (PyType_Ready(type) < 0)
        pybind11_fail("make_default_metaclass(): failure in PyType_Ready()!");

    return type;
}

// Assumption used in object_new, below.
static_assert(sizeof(size_t) == sizeof(void *), "pybind assumes a pointer can holder a size_t");

/** Instance creation function for all pybind11 types. It only allocates space for the C++ object
 * (or multiple objects, for Python-side inheritance from multiple pybind11 types), but doesn't call
 * the constructor -- an `__init__` function must do that.
 */
extern "C" PYBIND11_NOINLINE inline PyObject *pybind11_object_new(PyTypeObject *type, PyObject *, PyObject *) {
#if defined(PYPY_VERSION)
    // PyPy gets tp_basicsize wrong (issue 2482) under multiple inheritance when the first inherited
    // object is a a plain Python type (i.e. not derived from an extension type).  Fix it.
    ssize_t instance_size = static_cast<ssize_t>(sizeof(instance));
    if (type->tp_basicsize < instance_size) {
        type->tp_basicsize = instance_size;
    }
#endif
    PyObject *self = type->tp_alloc(type, 0);
    auto inst = reinterpret_cast<instance *>(self);
    auto types_it = type_info_iterator(type);
    auto &reg_inst = get_internals().registered_instances;

    // Simple path: no python-side multiple inheritance
    if (!types_it.all) {
        // Single type, so [b][v*][h] allocation
        auto &tinfo = *types_it;
        inst->values_and_holders = (void **) ::operator new(
                sizeof(void *) * (2 + tinfo->holder_size_in_ptrs));
        inst->values_and_holders[0] = 0;
        inst->values_and_holders[1] = ::operator new(tinfo->type_size);
        reg_inst.emplace(inst->values_and_holders[1], self);
    }
    else { // multiple base types
        // Allocate: [bb...][v1*][h1][v2*][h2]... where [vN*] is a value pointer, [hN] is the
        // (uninitialized) holder instance for value N, and [bb...] is a bitfield that tracks
        // whether the associated holder has been initialized.
        auto &all = *types_it.all;
        size_t n_types = all.size();
        constexpr size_t bits_per_chunk = 8 * sizeof(void *);
        const size_t flag_ptrs = n_types / bits_per_chunk + (n_types % bits_per_chunk != 0);
        size_t space = flag_ptrs;
        for (auto tinfo : all) {
            space += sizeof(void *);
            space += tinfo->holder_size_in_ptrs;
        }

        //pybind11::allocator<void *> alloc;
        inst->values_and_holders = (void **) ::operator new(space * sizeof(void *));
        for (size_t i = 0; i < flag_ptrs; i++) inst->values_and_holders[i] = 0;

        // FIXME: could we make the default holder do destruction without allocation, and then allocate
        // values instead of value pointers, when using these default holders?
        // (I don't think it's possible)
        size_t pos = flag_ptrs;
        for (auto &tinfo : all) {
            inst->values_and_holders[pos] = ::operator new(tinfo->type_size);
            reg_inst.emplace(inst->values_and_holders[pos], self);
            pos += 1 + tinfo->holder_size_in_ptrs;
        }
    }
    inst->owned = true;

    return self;
}

/// An `__init__` function constructs the C++ object. Users should provide at least one
/// of these using `py::init` or directly with `.def(__init__, ...)`. Otherwise, the
/// following default function will be used which simply throws an exception.
extern "C" inline int pybind11_object_init(PyObject *self, PyObject *, PyObject *) {
    PyTypeObject *type = Py_TYPE(self);
    std::string msg;
#if defined(PYPY_VERSION)
    msg += handle((PyObject *) type).attr("__module__").cast<std::string>() + ".";
#endif
    msg += type->tp_name;
    msg += ": No constructor defined!";
    PyErr_SetString(PyExc_TypeError, msg.c_str());
    return -1;
}

/// Instance destructor function for all pybind11 types. It calls `type_info.dealloc`
/// to destroy the C++ object itself, while the rest is Python bookkeeping.
extern "C" inline void pybind11_object_dealloc(PyObject *self) {
    auto instance = reinterpret_cast<detail::instance *>(self);

    auto &registered_instances = get_internals().registered_instances;
    if (instance->values_and_holders) {
        auto self_type = Py_TYPE(self);
        auto types_it = type_info_iterator(Py_TYPE(self));

        constexpr size_t bits_per_chunk = 8 * sizeof(void *);
        const size_t n_types = types_it.size;
        size_t pos = n_types / bits_per_chunk + (n_types % bits_per_chunk != 0);
        size_t i = 0;
        for (; types_it != type_info_iterator(); ++types_it) {
            auto &tinfo = *types_it;
            if (instance->values_and_holders[pos]) {
                value_and_holder v_h(instance->values_and_holders, pos, i);
                if (v_h.holder_constructed())
                    tinfo->destroy_holder(v_h);
                else if (instance->owned)
                    ::operator delete(v_h.value_ptr<void>());

                auto range = registered_instances.equal_range(instance->values_and_holders[pos]);
                bool found = false;
                for (auto it = range.first; it != range.second; ++it) {
                    if (self_type == Py_TYPE(it->second)) {
                        registered_instances.erase(it);
                        found = true;
                        break;
                    }
                }
                if (!found)
                    pybind11_fail("pybind11_object_dealloc(): Tried to deallocate unregistered instance!");
            }

            pos += 1 + tinfo->holder_size_in_ptrs;
            i++;
        }
        ::operator delete(instance->values_and_holders);
    }

    if (instance->weakrefs)
        PyObject_ClearWeakRefs(self);

    PyObject **dict_ptr = _PyObject_GetDictPtr(self);
    if (dict_ptr)
        Py_CLEAR(*dict_ptr);

    Py_TYPE(self)->tp_free(self);
}

/** Create the type which can be used as a common base for all classes.  This is
    needed in order to satisfy Python's requirements for multiple inheritance.
    Return value: New reference. */
inline PyObject *make_object_base_type(PyTypeObject *metaclass) {
    auto name_obj = reinterpret_steal<object>(PYBIND11_FROM_STRING("pybind11_object"));

    /* Danger zone: from now (and until PyType_Ready), make sure to
       issue no Python C API calls which could potentially invoke the
       garbage collector (the GC will call type_traverse(), which will in
       turn find the newly constructed type in an invalid state) */
    auto heap_type = (PyHeapTypeObject *) metaclass->tp_alloc(metaclass, 0);
    if (!heap_type)
        pybind11_fail("make_object_base_type(): error allocating type!");

    heap_type->ht_name = name_obj.inc_ref().ptr();
#if PY_MAJOR_VERSION >= 3 && PY_MINOR_VERSION >= 3
    heap_type->ht_qualname = name_obj.inc_ref().ptr();
#endif

    auto type = &heap_type->ht_type;
    type->tp_name = "pybind11_object";
    type->tp_base = &PyBaseObject_Type;
    type->tp_basicsize = static_cast<ssize_t>(sizeof(instance));
    type->tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HEAPTYPE;

    type->tp_new = pybind11_object_new;
    type->tp_init = pybind11_object_init;
    type->tp_dealloc = pybind11_object_dealloc;

    /* Support weak references (needed for the keep_alive feature) */
    type->tp_weaklistoffset = offsetof(instance, weakrefs);

    if (PyType_Ready(type) < 0)
        pybind11_fail("PyType_Ready failed in make_object_base_type():" + error_string());

    assert(!PyType_HasFeature(type, Py_TPFLAGS_HAVE_GC));
    return (PyObject *) heap_type;
}

/// dynamic_attr: Support for `d = instance.__dict__`.
extern "C" inline PyObject *pybind11_get_dict(PyObject *self, void *) {
    PyObject *&dict = *_PyObject_GetDictPtr(self);
    if (!dict)
        dict = PyDict_New();
    Py_XINCREF(dict);
    return dict;
}

/// dynamic_attr: Support for `instance.__dict__ = dict()`.
extern "C" inline int pybind11_set_dict(PyObject *self, PyObject *new_dict, void *) {
    if (!PyDict_Check(new_dict)) {
        PyErr_Format(PyExc_TypeError, "__dict__ must be set to a dictionary, not a '%.200s'",
                     Py_TYPE(new_dict)->tp_name);
        return -1;
    }
    PyObject *&dict = *_PyObject_GetDictPtr(self);
    Py_INCREF(new_dict);
    Py_CLEAR(dict);
    dict = new_dict;
    return 0;
}

/// dynamic_attr: Allow the garbage collector to traverse the internal instance `__dict__`.
extern "C" inline int pybind11_traverse(PyObject *self, visitproc visit, void *arg) {
    PyObject *&dict = *_PyObject_GetDictPtr(self);
    Py_VISIT(dict);
    return 0;
}

/// dynamic_attr: Allow the GC to clear the dictionary.
extern "C" inline int pybind11_clear(PyObject *self) {
    PyObject *&dict = *_PyObject_GetDictPtr(self);
    Py_CLEAR(dict);
    return 0;
}

/// Give instances of this type a `__dict__` and opt into garbage collection.
inline void enable_dynamic_attributes(PyHeapTypeObject *heap_type) {
    auto type = &heap_type->ht_type;
#if defined(PYPY_VERSION)
    pybind11_fail(std::string(type->tp_name) + ": dynamic attributes are "
                                               "currently not supported in "
                                               "conjunction with PyPy!");
#endif
    type->tp_flags |= Py_TPFLAGS_HAVE_GC;
    type->tp_dictoffset = type->tp_basicsize; // place dict at the end
    type->tp_basicsize += sizeof(PyObject *); // and allocate enough space for it
    type->tp_traverse = pybind11_traverse;
    type->tp_clear = pybind11_clear;

    static PyGetSetDef getset[] = {
        {const_cast<char*>("__dict__"), pybind11_get_dict, pybind11_set_dict, nullptr, nullptr},
        {nullptr, nullptr, nullptr, nullptr, nullptr}
    };
    type->tp_getset = getset;
}

/// buffer_protocol: Fill in the view as specified by flags.
extern "C" inline int pybind11_getbuffer(PyObject *obj, Py_buffer *view, int flags) {
    auto tinfo = get_type_info(Py_TYPE(obj));
    if (view == nullptr || obj == nullptr || !tinfo || !tinfo->get_buffer) {
        if (view)
            view->obj = nullptr;
        PyErr_SetString(PyExc_BufferError, "generic_type::getbuffer(): Internal error");
        return -1;
    }
    memset(view, 0, sizeof(Py_buffer));
    buffer_info *info = tinfo->get_buffer(obj, tinfo->get_buffer_data);
    view->obj = obj;
    view->ndim = 1;
    view->internal = info;
    view->buf = info->ptr;
    view->itemsize = (ssize_t) info->itemsize;
    view->len = view->itemsize;
    for (auto s : info->shape)
        view->len *= s;
    if ((flags & PyBUF_FORMAT) == PyBUF_FORMAT)
        view->format = const_cast<char *>(info->format.c_str());
    if ((flags & PyBUF_STRIDES) == PyBUF_STRIDES) {
        view->ndim = (int) info->ndim;
        view->strides = (ssize_t *) &info->strides[0];
        view->shape = (ssize_t *) &info->shape[0];
    }
    Py_INCREF(view->obj);
    return 0;
}

/// buffer_protocol: Release the resources of the buffer.
extern "C" inline void pybind11_releasebuffer(PyObject *, Py_buffer *view) {
    delete (buffer_info *) view->internal;
}

/// Give this type a buffer interface.
inline void enable_buffer_protocol(PyHeapTypeObject *heap_type) {
    heap_type->ht_type.tp_as_buffer = &heap_type->as_buffer;
#if PY_MAJOR_VERSION < 3
    heap_type->ht_type.tp_flags |= Py_TPFLAGS_HAVE_NEWBUFFER;
#endif

    heap_type->as_buffer.bf_getbuffer = pybind11_getbuffer;
    heap_type->as_buffer.bf_releasebuffer = pybind11_releasebuffer;
}

/** Create a brand new Python type according to the `type_record` specification.
    Return value: New reference. */
inline PyObject* make_new_python_type(const type_record &rec) {
    auto name = reinterpret_steal<object>(PYBIND11_FROM_STRING(rec.name));

#if PY_MAJOR_VERSION >= 3 && PY_MINOR_VERSION >= 3
    auto ht_qualname = name;
    if (rec.scope && hasattr(rec.scope, "__qualname__")) {
        ht_qualname = reinterpret_steal<object>(
            PyUnicode_FromFormat("%U.%U", rec.scope.attr("__qualname__").ptr(), name.ptr()));
    }
#endif

    object module;
    if (rec.scope) {
        if (hasattr(rec.scope, "__module__"))
            module = rec.scope.attr("__module__");
        else if (hasattr(rec.scope, "__name__"))
            module = rec.scope.attr("__name__");
    }

#if !defined(PYPY_VERSION)
    const auto full_name = module ? str(module).cast<std::string>() + "." + rec.name
                                  : std::string(rec.name);
#else
    const auto full_name = std::string(rec.name);
#endif

    char *tp_doc = nullptr;
    if (rec.doc && options::show_user_defined_docstrings()) {
        /* Allocate memory for docstring (using PyObject_MALLOC, since
           Python will free this later on) */
        size_t size = strlen(rec.doc) + 1;
        tp_doc = (char *) PyObject_MALLOC(size);
        memcpy((void *) tp_doc, rec.doc, size);
    }

    auto &internals = get_internals();
    auto bases = tuple(rec.bases);
    auto base = (bases.size() == 0) ? internals.instance_base
                                    : bases[0].ptr();

    /* Danger zone: from now (and until PyType_Ready), make sure to
       issue no Python C API calls which could potentially invoke the
       garbage collector (the GC will call type_traverse(), which will in
       turn find the newly constructed type in an invalid state) */
    auto metaclass = rec.metaclass.ptr() ? (PyTypeObject *) rec.metaclass.ptr()
                                         : internals.default_metaclass;

    auto heap_type = (PyHeapTypeObject *) metaclass->tp_alloc(metaclass, 0);
    if (!heap_type)
        pybind11_fail(std::string(rec.name) + ": Unable to create type object!");

    heap_type->ht_name = name.release().ptr();
#if PY_MAJOR_VERSION >= 3 && PY_MINOR_VERSION >= 3
    heap_type->ht_qualname = ht_qualname.release().ptr();
#endif

    auto type = &heap_type->ht_type;
    type->tp_name = strdup(full_name.c_str());
    type->tp_doc = tp_doc;
    type->tp_base = (PyTypeObject *) handle(base).inc_ref().ptr();
    type->tp_basicsize = static_cast<ssize_t>(sizeof(instance));
    //type->tp_itemsize = 16;
    if (bases.size() > 0)
        type->tp_bases = bases.release().ptr();

    /* Don't inherit base __init__ */
    type->tp_init = pybind11_object_init;

    /* Supported protocols */
    type->tp_as_number = &heap_type->as_number;
    type->tp_as_sequence = &heap_type->as_sequence;
    type->tp_as_mapping = &heap_type->as_mapping;

    /* Flags */
    type->tp_flags |= Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HEAPTYPE;
#if PY_MAJOR_VERSION < 3
    type->tp_flags |= Py_TPFLAGS_CHECKTYPES;
#endif

    if (rec.dynamic_attr)
        enable_dynamic_attributes(heap_type);

    if (rec.buffer_protocol)
        enable_buffer_protocol(heap_type);

    if (PyType_Ready(type) < 0)
        pybind11_fail(std::string(rec.name) + ": PyType_Ready failed (" + error_string() + ")!");

    assert(rec.dynamic_attr ? PyType_HasFeature(type, Py_TPFLAGS_HAVE_GC)
                            : !PyType_HasFeature(type, Py_TPFLAGS_HAVE_GC));

    /* Register type with the parent scope */
    if (rec.scope)
        setattr(rec.scope, rec.name, (PyObject *) type);

    if (module) // Needed by pydoc
        setattr((PyObject *) type, "__module__", module);

    return (PyObject *) type;
}

NAMESPACE_END(detail)
NAMESPACE_END(pybind11)
