/*
    tests/test_factory_constructors.cpp -- tests construction from a factory function
                                           via py::init_factory()

    Copyright (c) 2017 Jason Rhinelander <jason@imaginary.ca>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#include "pybind11_tests.h"
#include "constructor_stats.h"
#include <cmath>
#include <pybind11/factory.h>

// Classes for testing python construction via C++ factory function:
// Not publically constructible, copyable, or movable:
class TestFactory1 {
    friend class TestFactoryHelper;
    TestFactory1() : value("(empty)") { print_default_created(this); }
    TestFactory1(int v) : value(std::to_string(v)) { print_created(this, value); }
    TestFactory1(std::string v) : value(std::move(v)) { print_created(this, value); }
    TestFactory1(TestFactory1 &&) = delete;
    TestFactory1(const TestFactory1 &) = delete;
    TestFactory1 &operator=(TestFactory1 &&) = delete;
    TestFactory1 &operator=(const TestFactory1 &) = delete;
public:
    std::string value;
    ~TestFactory1() { print_destroyed(this); }
};
// Non-public construction, but moveable:
class TestFactory2 {
    friend class TestFactoryHelper;
    TestFactory2() : value("(empty2)") { print_default_created(this); }
    TestFactory2(int v) : value(std::to_string(v)) { print_created(this, value); }
    TestFactory2(std::string v) : value(std::move(v)) { print_created(this, value); }
public:
    TestFactory2(TestFactory2 &&m) { value = std::move(m.value); print_move_created(this); }
    TestFactory2 &operator=(TestFactory2 &&m) { value = std::move(m.value); print_move_assigned(this); return *this; }
    std::string value;
    ~TestFactory2() { print_destroyed(this); }
};
// Mixed direct/factory construction:
class TestFactory3 {
protected:
    friend class TestFactoryHelper;
    TestFactory3() : value("(empty3)") { print_default_created(this); }
    TestFactory3(int v) : value(std::to_string(v)) { print_created(this, value); }
public:
    TestFactory3(std::string v) : value(std::move(v)) { print_created(this, value); }
    TestFactory3(TestFactory3 &&m) { value = std::move(m.value); print_move_created(this); }
    TestFactory3 &operator=(TestFactory3 &&m) { value = std::move(m.value); print_move_assigned(this); return *this; }
    std::string value;
    virtual ~TestFactory3() { print_destroyed(this); }
};
// Inheritance test
class TestFactory4 : public TestFactory3 {
public:
    TestFactory4() : TestFactory3() { print_default_created(this); }
    TestFactory4(int v) : TestFactory3(v) { print_created(this, v); }
    virtual ~TestFactory4() { print_destroyed(this); }
};
// Another class for an invalid downcast test
class TestFactory5 : public TestFactory3 {
public:
    TestFactory5(int i) : TestFactory3(i) { print_created(this, i); }
    virtual ~TestFactory5() { print_destroyed(this); }
};

struct NinetyNine {};
class TestFactory6 {
protected:
    int value;
    bool alias = false;
public:
    TestFactory6(int i) : value{i} { print_created(this, i); }
    TestFactory6(TestFactory6 &&f) { print_move_created(this); value = f.value; alias = f.alias; }
    TestFactory6(const TestFactory6 &f) { print_copy_created(this); value = f.value; alias = f.alias; }
    // Implicit conversion not supported by alias:
    TestFactory6(NinetyNine) : TestFactory6(99) {}
    virtual ~TestFactory6() { print_destroyed(this); }
    virtual int get() { return value; }
    bool has_alias() { return alias; }
};
class PyTF6 : public TestFactory6 {
public:
    PyTF6(int i) : TestFactory6(i) { alias = true; print_created(this, i); }
    // Allow implicit conversion from std::string:
    PyTF6(std::string s) : TestFactory6((int) s.size()) { alias = true; print_created(this, s); }
    PyTF6(PyTF6 &&f) : TestFactory6(std::move(f)) { print_move_created(this); }
    PyTF6(const PyTF6 &f) : TestFactory6(f) { print_copy_created(this); }
    virtual ~PyTF6() { print_destroyed(this); }
    int get() override { PYBIND11_OVERLOAD(int, TestFactory6, get, /*no args*/); }
};

// Stash leaked values here so we can clean up at the end of the test:
py::object leak1;
TestFactory3 *leak2, *leak3;
class TestFactoryHelper {
public:
    // Return via pointer:
    static TestFactory1 *construct1() { return new TestFactory1(); }
    // Holder:
    static std::unique_ptr<TestFactory1> construct1(int a) { return std::unique_ptr<TestFactory1>(new TestFactory1(a)); }
    // pointer again
    static TestFactory1 *construct1(std::string a) { return new TestFactory1(a); }

    // pointer:
    static TestFactory2 *construct2() { return new TestFactory2(); }
    // holder:
    static std::unique_ptr<TestFactory2> construct2(int a) { return std::unique_ptr<TestFactory2>(new TestFactory2(a)); }
    // by value moving:
    static TestFactory2 construct2(std::string a) { return TestFactory2(a); }

    // pointer:
    static TestFactory3 *construct3() { return new TestFactory3(); }
    // holder:
    static std::shared_ptr<TestFactory3> construct3(int a) { return std::shared_ptr<TestFactory3>(new TestFactory3(a)); }
    // by object:
    static py::object construct3(double a) {
        return py::cast(new TestFactory3((int) std::lround(a)), py::return_value_policy::take_ownership); }

    // Invalid values:
    // Multiple references:
    static py::object construct_bad3a(double v) {
        auto o = construct3(v);
        leak1 = o;
        return o;
    }
    // Unowned pointer:
    static py::object construct_bad3b(int v) {
        leak2 = new TestFactory3(v);
        return py::cast(leak2, py::return_value_policy::reference);
    }
};

test_initializer factory_constructors([](py::module &m) {

    // Define various trivial types to allow simpler overload resolution:
    py::module m_tag = m.def_submodule("tag");
#define MAKE_TAG_TYPE(Name) \
    struct Name##_tag {}; \
    py::class_<Name##_tag>(m_tag, #Name "_tag").def(py::init<>()); \
    m_tag.attr(#Name) = py::cast(Name##_tag{})
    MAKE_TAG_TYPE(pointer);
    MAKE_TAG_TYPE(unique_ptr);
    MAKE_TAG_TYPE(move);
    MAKE_TAG_TYPE(object);
    MAKE_TAG_TYPE(shared_ptr);
    MAKE_TAG_TYPE(raw_object);
    MAKE_TAG_TYPE(multiref);
    MAKE_TAG_TYPE(unowned);
    MAKE_TAG_TYPE(derived);
    MAKE_TAG_TYPE(TF4);
    MAKE_TAG_TYPE(TF5);
    MAKE_TAG_TYPE(null_ptr);
    MAKE_TAG_TYPE(base);
    MAKE_TAG_TYPE(invalid_base);
    MAKE_TAG_TYPE(alias);
    MAKE_TAG_TYPE(unaliasable);

    py::class_<TestFactory1>(m, "TestFactory1")
        .def(py::init_factory([](pointer_tag, int v) { return TestFactoryHelper::construct1(v); }))
        .def(py::init_factory([](unique_ptr_tag, std::string v) { return TestFactoryHelper::construct1(v); }))
        .def(py::init_factory([](pointer_tag) { return TestFactoryHelper::construct1(); }))
        // Takes a python function that returns the instance:
        .def(py::init_factory([](py::function f) { return f(123); }))
        // Sets a fallback python factory function (gets called if none of the above match):
        .def_static("set_ctor_fallback", [](py::function f) {
            auto tf1 = py::reinterpret_borrow<py::class_<TestFactory1>>(
                    py::module::import("pybind11_tests").attr("TestFactory1"));
            tf1.def(py::init_factory([f]() { return f(); }));
        })
        .def_readwrite("value", &TestFactory1::value)
        ;
    py::class_<TestFactory2>(m, "TestFactory2")
        .def(py::init_factory([](pointer_tag, int v) { return TestFactoryHelper::construct2(v); }))
        .def(py::init_factory([](unique_ptr_tag, std::string v) { return TestFactoryHelper::construct2(v); }))
        .def(py::init_factory([](move_tag) { return TestFactoryHelper::construct2(); }))
        .def_readwrite("value", &TestFactory2::value)
        ;
    int c = 1;
    // Stateful & reused:
    auto c4a = [c](pointer_tag, TF4_tag, int a) { return new TestFactory4(a);};
    auto c4b = [](object_tag, TF4_tag, int a) {
        return py::cast(new TestFactory4(a), py::return_value_policy::take_ownership); };

    py::class_<TestFactory3, std::shared_ptr<TestFactory3>>(m, "TestFactory3")
        .def(py::init_factory([](pointer_tag, int v) { return TestFactoryHelper::construct3(v); }))
        .def(py::init_factory([](shared_ptr_tag) { return TestFactoryHelper::construct3(); }))
        .def("__init__", [](TestFactory3 &self, std::string v) { new (&self) TestFactory3(v); }) // regular ctor
        // Stateful lambda returning py::object:
        .def(py::init_factory([c](object_tag, int v) { return TestFactoryHelper::construct3(double(v + c)); }))
        .def(py::init_factory([](raw_object_tag, double v) {
            auto o = TestFactoryHelper::construct3(v); return o.release().ptr(); }))
        .def(py::init_factory([](multiref_tag, double v) { return TestFactoryHelper::construct_bad3a(v); })) // multi-ref object
        .def(py::init_factory([](unowned_tag, int v) { return TestFactoryHelper::construct_bad3b(v); })) // unowned ptr
        // wrong type returned (should trigger static_assert failure if uncommented):
        //.def(py::init_factory([](double a, int b) { return TestFactoryHelper::construct2((int) (a + b)); }))

        // factories returning a derived type:
        .def(py::init_factory(c4a)) // derived ptr
        .def(py::init_factory(c4b)) // derived py::object: fails; object up/down-casting currently not supported
        .def(py::init_factory([](pointer_tag, TF5_tag, int a) { return new TestFactory5(a); }))
        .def(py::init_factory([](pointer_tag, TF5_tag, int a) { return new TestFactory5(a); }))
        // derived shared ptr:
        .def(py::init_factory([](shared_ptr_tag, TF4_tag, int a) { return std::make_shared<TestFactory4>(a); }))
        .def(py::init_factory([](shared_ptr_tag, TF5_tag, int a) { return std::make_shared<TestFactory5>(a); }))

        // Returns nullptr:
        .def(py::init_factory([](null_ptr_tag) { return (TestFactory3 *) nullptr; }))

        .def_readwrite("value", &TestFactory3::value)
        .def_static("cleanup_leaks", []() {
            leak1 = py::object();
            // Make sure they aren't referenced before deleting them:
            if (py::detail::get_internals().registered_instances.count(leak2) == 0)
                delete leak2;
            if (py::detail::get_internals().registered_instances.count(leak2) == 0)
                delete leak3;
        })
        ;
    py::class_<TestFactory4, TestFactory3, std::shared_ptr<TestFactory4>>(m, "TestFactory4")
        .def(py::init_factory(c4a)) // pointer
        .def(py::init_factory(c4b)) // py::object
        // Valid downcasting test:
        .def(py::init_factory([](shared_ptr_tag, base_tag, int a) {
            return std::shared_ptr<TestFactory3>(new TestFactory4(a)); }))
        .def(py::init_factory([](pointer_tag, base_tag, int a) {
            return (TestFactory3 *) new TestFactory4(a); }))
        // Invalid downcasting test:
        .def(py::init_factory([](shared_ptr_tag, invalid_base_tag, int a) {
            return std::shared_ptr<TestFactory3>(new TestFactory5(a)); }))
        .def(py::init_factory([](pointer_tag, invalid_base_tag, int a) {
            return (TestFactory3 *) new TestFactory5(a); }))
        ;

    // Doesn't need to be registered, but registering makes getting ConstructorStats easier:
    py::class_<TestFactory5, TestFactory3, std::shared_ptr<TestFactory5>>(m, "TestFactory5");

    // Alias testing
    py::class_<TestFactory6, PyTF6>(m, "TestFactory6")
        .def(py::init_factory([](int i) { return i; }))
        .def(py::init_factory([](std::string s) { return s; }))
        .def(py::init_factory([](base_tag, int i) { return TestFactory6(i); }))
        .def(py::init_factory([](alias_tag, int i) { return PyTF6(i); }))
        .def(py::init_factory([](alias_tag, pointer_tag, int i) { return new PyTF6(i); }))
        .def(py::init_factory([](base_tag, pointer_tag, int i) { return new TestFactory6(i); }))
        .def(py::init_factory([](base_tag, alias_tag, pointer_tag, int i) { return (TestFactory6 *) new PyTF6(i); }))
        .def(py::init_factory([](unaliasable_tag) { return NinetyNine(); }))

        .def("get", &TestFactory6::get)
        .def("has_alias", &TestFactory6::has_alias)

        .def_static("get_cstats", &ConstructorStats::get<TestFactory6>, py::return_value_policy::reference)
        .def_static("get_alias_cstats", &ConstructorStats::get<PyTF6>, py::return_value_policy::reference)
        ;
});
