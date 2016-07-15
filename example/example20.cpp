/*
    example/example20.cpp -- implicit conversion between types

    Copyright (c) 2016 Jason Rhinelander <jason@imaginary.ca>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#include "example.h"
#include <cmath>

/// Objects to test implicit conversion
class Ex20_A {
public:
    // Implicit conversion *from* double
    Ex20_A(double v) : value{v} {}
    // Default constructor
    Ex20_A() : Ex20_A(42.0) {}
    // Implicit conversion *to* double
    virtual operator double() const { std::cout << "Ex20_A double conversion operator" << std::endl; return value; }
private:
    double value;
};
class Ex20_E;
class Ex20_B : public Ex20_A {
public:
    // Implicit conversion to Ex20_E
    operator Ex20_E() const;
};
class Ex20_C : public Ex20_B {
public:
    // Implicit conversion to double
    virtual operator double() const override { return 3.14159265358979323846; }
    // Implicit conversion to string
    operator std::string() const { return "pi"; }
};
class Ex20_D : public Ex20_A {
public:
    // Implicit conversion to double
    virtual operator double() const override { return 2.71828182845904523536; }
    // Implicit conversion to string
    operator std::string() const { return "e"; }
};
// This class won't be registered with pybind11, but a function accepting it will be--the function
// can only be called with arguments that are implicitly convertible to Ex20_E
class Ex20_E {
public:
    Ex20_E() = delete;
    Ex20_E(const Ex20_E &e) : value{e.value} { std::cout << "Ex20_E @ " << this << " copy constructor" << std::endl; }
    Ex20_E(Ex20_E &&e) : value{std::move(e.value)} { std::cout << "Ex20_E @ " << this << " move constructor" << std::endl; }
    ~Ex20_E() { std::cout << "Ex20_E @ " << this << " destructor" << std::endl; }
    // explicit constructors should not be called by implicit conversion:
    explicit Ex20_E(double d) : value{d} { std::cout << "Ex20_E @ " << this << " double constructor" << std::endl; }
    explicit Ex20_E(const Ex20_A &a) : value{(double)a / 3.0} { std::cout << "Ex20_E @ " << this << " explicit Ex20_A constructor" << std::endl; }
    // Convertible implicitly from D:
    Ex20_E(const Ex20_D &d) : value{3*d} { std::cout << "Ex20_E @ " << this << " implicit Ex20_D constructor" << std::endl; }
    // Implicit conversion to double:
    operator double() const { std::cout << "Ex20_E double conversion operator" << std::endl; return value; }
private:
    double value;
};
Ex20_B::operator Ex20_E() const { std::cout << "Ex20_B @ " << this << " Ex20_E conversion operator" << std::endl; return Ex20_E(2*(double)(*this)); }
// Class without a move constructor (just to be sure we don't depend on a move constructor).
// Unlike the above, we *will* expose this one to python, but will declare its
// implicitly_convertible before registering it, which will result in C++ (not python) type
// conversion.
class Ex20_F {
public:
    Ex20_F() : value{99.0} { std::cout << "Ex20_F @ " << this << " default constructor" << std::endl; }
    Ex20_F(const Ex20_A &a) : value{(double)a*1000} { std::cout << "Ex20_F @ " << this << " Ex20_A conversion constructor" << std::endl; }
    Ex20_F(const Ex20_F &f) : value{f.value} { std::cout << "Ex20_F @ " << this << " copy constructor" << std::endl; }
    ~Ex20_F() { std::cout << "Ex20_F @ " << this << " destructor" << std::endl; }
    operator double() const { return value; }
private:
    double value;
};

class Ex20_G1 {
public:
    operator long() const { return 111; }
};
class Ex20_G2 : public Ex20_G1 {
public:
    operator long() const { return 222; }
};
class Ex20_G3 {
public:
    operator long() const { return 333; }
};
class Ex20_G4 : public Ex20_G3 {
public:
    operator long() const { return 444; }
};

// Implicit base class casting
class Ex20_H1 {
public:
    explicit Ex20_H1(int value) : value{value} {}
    int val() const { return value; }
protected:
    int value{-1};
};
class Ex20_H2 : public Ex20_H1 {
public:
    Ex20_H2(int value) : Ex20_H1(value) {}
    void increment() { value++; }
};
class Ex20_H3 {};
class Ex20_H4 : public Ex20_H3, public Ex20_H2 {
public:
    Ex20_H4(int value) : Ex20_H2(value) {}
};




void print_double(double d) { std::cout << d << std::endl; }
void print_long(long l) { std::cout << l << std::endl; }
void print_string(const std::string &s) { std::cout << s << std::endl; }
void print_ex20e(const Ex20_E &e) { std::cout << (double) e << std::endl; }
void print_ex20f(const Ex20_F &f) { std::cout << (double) f << std::endl; }

void init_ex20(py::module &m) {

    py::class_<Ex20_A> a(m, "Ex20_A");
    a.def(py::init<>());
    a.def(py::init<double>());

    // We can construct a Ex20_A from a double:
    py::implicitly_convertible<py::float_, Ex20_A>();

    // It can also be implicitly to a double:
    py::implicitly_convertible<Ex20_A, double>();


    py::class_<Ex20_B> b(m, "Ex20_B", a);
    b.def(py::init<>());

    py::class_<Ex20_C> c(m, "Ex20_C", b);
    c.def(py::init<>());

    py::class_<Ex20_D> d(m, "Ex20_D", a);
    d.def(py::init<>());

    // NB: don't need to implicitly declare Ex20_{B,C} as convertible to double: they automatically
    // get that since we told pybind11 they inherit from A
    py::implicitly_convertible<Ex20_C, std::string>();
    py::implicitly_convertible<Ex20_D, std::string>();

    // NB: Ex20_E is a non-pybind-registered class:
    //
    // This should fail: Ex20_A is *not* C++ implicitly convertible to Ex20_E (the constructor is
    // marked explicit):
    try {
        py::implicitly_convertible<Ex20_A, Ex20_E>();
        std::cout << "py::implicitly_convertible<Ex20_A, Ex20_E>() should have thrown, but didn't!" << std::endl;
    }
    catch (std::runtime_error) {}

    py::implicitly_convertible<Ex20_B, Ex20_E>();
    // This isn't needed, since pybind knows C inherits from B
    //py::implicitly_convertible<Ex20_C, Ex20_E>();
    py::implicitly_convertible<Ex20_D, Ex20_E>();

    m.def("print_double", &print_double);
    m.def("print_long", &print_long);
    m.def("print_string", &print_string);
    m.def("print_ex20e", &print_ex20e);
    m.def("print_ex20f", &print_ex20f);

    // Here's how we can get C++-level implicit conversion even with a pybind-registered type: tell
    // pybind11 that the type is convertible to F before registering F:
    py::implicitly_convertible<Ex20_A, Ex20_F>();

    py::class_<Ex20_F> f(m, "Ex20_F");
    // We allow Ex20_F to be constructed in Python, but don't provide a conversion constructor from
    // Ex20_A.  C++ has an implicit one, however, that we told pybind11 about above.  In practice
    // this means we are allowed to pass Ex20_A instances to functions taking Ex20_F arguments, but
    // aren't allowed to write `ex20_func(Ex20_F(a))` because the explicit conversion is
    // (intentionally) not exposed to python.  (Whether this is useful is really up to the
    // developer).
    f.def(py::init<>());

    py::class_<Ex20_G1> g1(m, "Ex20_G1");     g1.def(py::init<>());
    py::class_<Ex20_G2> g2(m, "Ex20_G2", g1); g2.def(py::init<>());
    py::class_<Ex20_G3> g3(m, "Ex20_G3");     g3.def(py::init<>());
    py::class_<Ex20_G4> g4(m, "Ex20_G4", g3); g4.def(py::init<>());
    // Make sure that the order we declare convertibility doesn't matter: i.e. the base class
    // conversions here (G1 and G3) should not be invoked for G2 and G4, regardless of the
    // implicitly convertible declaration order.
    py::implicitly_convertible<Ex20_G2, long>();
    py::implicitly_convertible<Ex20_G1, long>();
    py::implicitly_convertible<Ex20_G3, long>();
    py::implicitly_convertible<Ex20_G4, long>();

    // When implicitly_convertible is given a derived and base class, it should "convert" via base
    // pointer casting, i.e. NOT via creating a new object.
    py::class_<Ex20_H4> h4(m, "Ex20_H4");
    h4.def(py::init<int>());
    m.def("increment_h2", [](Ex20_H2 &h2) { h2.increment(); });
    m.def("print_h1", [](const Ex20_H1 &h1) { std::cout << h1.val() << "\n"; });
    py::implicitly_convertible<Ex20_H4, Ex20_H3>();
    py::implicitly_convertible<Ex20_H4, Ex20_H2>();
    py::implicitly_convertible<Ex20_H2, Ex20_H1>();
}
