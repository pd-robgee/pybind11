/*
    example/example18.cpp -- implicit conversion between types

    Copyright (c) 2016 Jason Rhinelander <jason@imaginary.ca>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#include "example.h"
#include <pybind11/implicit.h>
#include <cmath>

/// Objects to test implicit conversion
class Ex18_A {
public:
    // Implicit conversion *from* double
    Ex18_A(double v) : value{v} {}
    // Default constructor
    Ex18_A() : Ex18_A(42.0) {}
    // Implicit conversion *to* double
    virtual operator double() const { std::cout << "Ex18_A double conversion operator" << std::endl; return value; }
private:
    double value;
};
class Ex18_E;
class Ex18_B : public Ex18_A {
public:
    // Implicit conversion to Ex18_E
    operator Ex18_E() const;
};
class Ex18_C : public Ex18_B {
public:
    // Implicit conversion to double
    virtual operator double() const override { return 3.14159265358979323846; }
    // Implicit conversion to string
    operator std::string() const { return "pi"; }
};
class Ex18_D : public Ex18_A {
public:
    // Implicit conversion to double
    virtual operator double() const override { return 2.71828182845904523536; }
    // Implicit conversion to string
    operator std::string() const { return "e"; }
};
// This class won't be registered with pybind11, but a function accepting it will be--the function
// can only be called with arguments that are implicitly convertible to Ex18_E
class Ex18_E {
public:
    Ex18_E() = delete;
    Ex18_E(const Ex18_E &e) : value{e.value} { std::cout << "Ex18_E @ " << this << " copy constructor" << std::endl; }
    Ex18_E(Ex18_E &&e) : value{std::move(e.value)} { std::cout << "Ex18_E @ " << this << " move constructor" << std::endl; }
    ~Ex18_E() { std::cout << "Ex18_E @ " << this << " destructor" << std::endl; }
    // explicit constructors should not be called by implicit conversion:
    explicit Ex18_E(double d) : value{d} { std::cout << "Ex18_E @ " << this << " double constructor" << std::endl; }
    explicit Ex18_E(const Ex18_A &a) : value{(double)a / 3.0} { std::cout << "Ex18_E @ " << this << " explicit Ex18_A constructor" << std::endl; }
    // Convertible implicitly from D:
    Ex18_E(const Ex18_D &d) : value{3*d} { std::cout << "Ex18_E @ " << this << " implicit Ex18_D constructor" << std::endl; }
    // Implicit conversion to double:
    operator double() const { std::cout << "Ex18_E double conversion operator" << std::endl; return value; }
private:
    double value;
};
Ex18_B::operator Ex18_E() const { std::cout << "Ex18_B @ " << this << " Ex18_E conversion operator" << std::endl; return Ex18_E(2*(double)(*this)); }
// Class without a move constructor (just to be sure we don't depend on a move constructor).
// Unlike the above, we *will* expose this one to python, but will declare its
// implicitly_convertible before registering it, which will result in C++ (not python) type
// conversion.
class Ex18_F {
public:
    Ex18_F() : value{99.0} { std::cout << "Ex18_F @ " << this << " default constructor" << std::endl; }
    Ex18_F(const Ex18_A &a) : value{(double)a*1000} { std::cout << "Ex18_F @ " << this << " Ex18_A conversion constructor" << std::endl; }
    Ex18_F(const Ex18_F &f) : value{f.value} { std::cout << "Ex18_F @ " << this << " copy constructor" << std::endl; }
    ~Ex18_F() { std::cout << "Ex18_F @ " << this << " destructor" << std::endl; }
    operator double() const { return value; }
private:
    double value;
};

void print_double(double d) { std::cout << d << std::endl; }
void print_string(const std::string &s) { std::cout << s << std::endl; }
void print_ex18e(const Ex18_E &e) { std::cout << (double) e << std::endl; }
void print_ex18f(const Ex18_F &f) { std::cout << (double) f << std::endl; }

void init_ex18(py::module &m) {

    py::class_<Ex18_A> a(m, "Ex18_A");
    a.def(py::init<>());
    a.def(py::init<double>());

    // We can construct a Ex18_A from a double:
    py::implicitly_convertible<py::float_, Ex18_A>();

    // It can also be implicitly to a double:
    py::implicitly_convertible<Ex18_A, double>();


    py::class_<Ex18_B> b(m, "Ex18_B", a);
    b.def(py::init<>());

    py::class_<Ex18_C> c(m, "Ex18_C", b);
    c.def(py::init<>());

    py::class_<Ex18_D> d(m, "Ex18_D", a);
    d.def(py::init<>());

    // NB: don't need to implicitly declare Ex18_{B,C} as convertible to double: they automatically
    // get that since we told pybind11 they inherit from A
    py::implicitly_convertible<Ex18_C, std::string>();
    py::implicitly_convertible<Ex18_D, std::string>();

    // NB: Ex18_E is a non-pybind-registered class:
    //
    // This should fail: Ex18_A is *not* C++ implicitly convertible to Ex18_E (the constructor is
    // marked explicit):
    try {
        py::implicitly_convertible<Ex18_A, Ex18_E>();
        std::cout << "py::implicitly_convertible<Ex18_A, Ex18_E>() should have thrown, but didn't!" << std::endl;
    }
    catch (std::runtime_error) {}

    py::implicitly_convertible<Ex18_B, Ex18_E>();
    // This isn't needed, since pybind knows C inherits from B
    //py::implicitly_convertible<Ex18_C, Ex18_E>();
    py::implicitly_convertible<Ex18_D, Ex18_E>();

    m.def("print_double", &print_double);
    m.def("print_string", &print_string);
    m.def("print_ex18e", &print_ex18e);
    m.def("print_ex18f", &print_ex18f);

    // Here's how we can get C++-level implicit conversion even with a pybind-registered type: tell
    // pybind11 that the type is convertible to F before registering F:
    py::implicitly_convertible<Ex18_A, Ex18_F>();

    py::class_<Ex18_F> f(m, "Ex18_F");
    // We allow Ex18_F to be constructed in Python, but don't provide a conversion constructor from
    // Ex18_A.  C++ has an implicit one, however, that we told pybind11 about above.  In practice
    // this means we are allowed to pass Ex18_A instances to functions taking Ex18_F arguments, but
    // aren't allowed to write `ex18_func(Ex18_F(a))` because the explicit conversion is
    // (intentionally) not exposed to python.  (Whether this is useful is really up to the
    // developer).
    f.def(py::init<>());

}
