import pytest
from pybind11_tests import ConstructorStats


def test_init_factory_basic():
    """Tests py::init_factory() wrapper around various ways of returning the object"""
    from pybind11_tests import TestFactory1, TestFactory2, TestFactory3, tag

    cstats = [ConstructorStats.get(c) for c in [TestFactory1, TestFactory2, TestFactory3]]
    cstats[0].alive()  # force gc
    n_inst = ConstructorStats.detail_reg_inst()

    x1 = TestFactory1(tag.unique_ptr, 3)
    assert x1.value == "3"
    y1 = TestFactory1(tag.pointer)
    assert y1.value == "(empty)"
    z1 = TestFactory1("hi!")
    assert z1.value == "hi!"

    assert ConstructorStats.detail_reg_inst() == n_inst + 3

    x2 = TestFactory2(tag.move)
    assert x2.value == "(empty2)"
    y2 = TestFactory2(tag.pointer, 7)
    assert y2.value == "7"
    z2 = TestFactory2(tag.unique_ptr, "hi again")
    assert z2.value == "hi again"

    assert ConstructorStats.detail_reg_inst() == n_inst + 6

    x3 = TestFactory3(tag.shared_ptr)
    assert x3.value == "(empty3)"
    y3 = TestFactory3(tag.pointer, 42)
    assert y3.value == "42"
    z3 = TestFactory3("bye")
    assert z3.value == "bye"

    with pytest.raises(TypeError) as excinfo:
        TestFactory3(tag.null_ptr)
    assert (str(excinfo.value) ==
            "pybind11::init(): factory function returned nullptr")

    assert [i.alive() for i in cstats] == [3, 3, 3]
    assert ConstructorStats.detail_reg_inst() == n_inst + 9

    del x1, y2, y3, z3
    assert [i.alive() for i in cstats] == [2, 2, 1]
    assert ConstructorStats.detail_reg_inst() == n_inst + 5
    del x2, x3, y1, z1, z2
    assert [i.alive() for i in cstats] == [0, 0, 0]
    assert ConstructorStats.detail_reg_inst() == n_inst

    assert [i.values() for i in cstats] == [
        ["3", "hi!"],
        ["7", "hi again"],
        ["42", "bye"]
    ]
    assert [i.default_constructions for i in cstats] == [1, 1, 1]


def test_init_factory_casting():
    """Tests py::init_factory() wrapper with various upcasting and downcasting returns"""
    from pybind11_tests import TestFactory3, TestFactory4, TestFactory5, tag

    cstats = [ConstructorStats.get(c) for c in [TestFactory3, TestFactory4, TestFactory5]]
    cstats[0].alive()  # force gc
    n_inst = ConstructorStats.detail_reg_inst()

    # Construction from derived references:
    a = TestFactory3(tag.pointer, tag.TF4, 4)
    assert a.value == "4"
    b = TestFactory3(tag.shared_ptr, tag.TF4, 5)
    assert b.value == "5"
    c = TestFactory3(tag.pointer, tag.TF5, 6)
    assert c.value == "6"
    d = TestFactory3(tag.shared_ptr, tag.TF5, 7)
    assert d.value == "7"

    assert ConstructorStats.detail_reg_inst() == n_inst + 4

    # Construction from base references:
    e = TestFactory4(tag.pointer, tag.base, 8)
    assert e.value == "8"
    f = TestFactory4(tag.shared_ptr, tag.base, 9)
    assert f.value == "9"

    assert ConstructorStats.detail_reg_inst() == n_inst + 6
    assert [i.alive() for i in cstats] == [6, 4, 2]

    del a
    assert [i.alive() for i in cstats] == [5, 3, 2]
    assert ConstructorStats.detail_reg_inst() == n_inst + 5

    del b, c, e
    assert [i.alive() for i in cstats] == [2, 1, 1]
    assert ConstructorStats.detail_reg_inst() == n_inst + 2

    del f, d
    assert [i.alive() for i in cstats] == [0, 0, 0]
    assert ConstructorStats.detail_reg_inst() == n_inst

    # These return a base class which is *not* actually a TestFactory4 instance:
    with pytest.raises(TypeError) as excinfo:
        TestFactory4(tag.pointer, tag.invalid_base, -2)
    assert (str(excinfo.value) == "pybind11::init(): factory function failed: "
                                  "casting base pointer to instance pointer failed")
    with pytest.raises(TypeError) as excinfo:
        TestFactory4(tag.shared_ptr, tag.invalid_base, -3)
    assert (str(excinfo.value) ==
            "pybind11::init(): factory construction failed: base class shared_ptr is not "
            "a derived instance")

    assert [i.alive() for i in cstats] == [0, 0, 0]
    assert ConstructorStats.detail_reg_inst() == n_inst

    assert [i.values() for i in cstats] == [
        ["4", "5", "6", "7", "8", "9", "-2", "-3"],
        ["4", "5", "8", "9"],
        ["6", "7", "-2", "-3"]
    ]


def test_init_factory_alias():
    """Tests py::init_factory() wrapper with value conversions and alias types"""
    from pybind11_tests import TestFactory6, tag

    cstats = [TestFactory6.get_cstats(), TestFactory6.get_alias_cstats()]
    cstats[0].alive()  # force gc
    n_inst = ConstructorStats.detail_reg_inst()

    a = TestFactory6(tag.base, 1)
    assert a.get() == 1
    assert not a.has_alias()
    b = TestFactory6(tag.alias, "hi there")
    assert b.get() == 8
    assert b.has_alias()
    c = TestFactory6(tag.alias, 3)
    assert c.get() == 3
    assert c.has_alias()
    d = TestFactory6(tag.alias, tag.pointer, 4)
    assert d.get() == 4
    assert d.has_alias()
    e = TestFactory6(tag.base, tag.pointer, 5)
    assert e.get() == 5
    assert not e.has_alias()
    f = TestFactory6(tag.base, tag.alias, tag.pointer, 6)
    assert f.get() == 6
    assert f.has_alias()

    assert ConstructorStats.detail_reg_inst() == n_inst + 6
    assert [i.alive() for i in cstats] == [6, 4]

    del a, b, e
    assert [i.alive() for i in cstats] == [3, 3]
    assert ConstructorStats.detail_reg_inst() == n_inst + 3
    del f, c, d
    assert [i.alive() for i in cstats] == [0, 0]
    assert ConstructorStats.detail_reg_inst() == n_inst

    class MyTest(TestFactory6):
        def __init__(self, *args):
            TestFactory6.__init__(self, *args)

        def get(self):
            return -5 + TestFactory6.get(self)

    z = MyTest(tag.base, 123)
    assert z.get() == 118
    assert z.has_alias()
    y = MyTest(tag.alias, "why hello!")
    assert y.get() == 5
    assert y.has_alias()
    assert ConstructorStats.detail_reg_inst() == n_inst + 2
    assert [i.alive() for i in cstats] == [2, 2]
    del z, y
    assert [i.alive() for i in cstats] == [0, 0]
    assert ConstructorStats.detail_reg_inst() == n_inst

    assert [i.values() for i in cstats] == [
        ["1", "8", "3", "4", "5", "6", "123", "10"],
        ["hi there", "3", "4", "6", "move", "123", "why hello!"]
    ]


def test_init_factory_dual():
    """Tests init factory functions with dual main/alias factory functions"""
    from pybind11_tests import TestFactory7, tag

    cstats = [TestFactory7.get_cstats(), TestFactory7.get_alias_cstats()]
    cstats[0].alive()  # force gc
    n_inst = ConstructorStats.detail_reg_inst()

    class PythFactory7(TestFactory7):
        def get(self):
            return 100 + TestFactory7.get(self)

    a1 = TestFactory7(1)
    a2 = PythFactory7(2)
    assert a1.get() == 1
    assert a2.get() == 102
    assert not a1.has_alias()
    assert a2.has_alias()

    b1 = TestFactory7(tag.pointer, 3)
    b2 = PythFactory7(tag.pointer, 4)
    assert b1.get() == 3
    assert b2.get() == 104
    assert not b1.has_alias()
    assert b2.has_alias()

    c1 = TestFactory7(tag.mixed, 5)
    c2 = PythFactory7(tag.mixed, 6)
    assert c1.get() == 5
    assert c2.get() == 106
    assert not c1.has_alias()
    assert c2.has_alias()

    d1 = TestFactory7(tag.base, tag.pointer, 7)
    d2 = PythFactory7(tag.base, tag.pointer, 8)
    assert d1.get() == 7
    assert d2.get() == 108
    assert not d1.has_alias()
    assert d2.has_alias()

    # Both return an alias; the second multiplies the value by 10:
    e1 = TestFactory7(tag.alias, tag.pointer, 9)
    e2 = PythFactory7(tag.alias, tag.pointer, 10)
    assert e1.get() == 9
    assert e2.get() == 200
    assert e1.has_alias()
    assert e2.has_alias()

    f1 = TestFactory7(tag.shared_ptr, tag.base, 11)
    f2 = PythFactory7(tag.shared_ptr, tag.base, 12)
    assert f1.get() == 11
    assert f2.get() == 112
    assert not f1.has_alias()
    assert f2.has_alias()

    g1 = TestFactory7(tag.shared_ptr, tag.invalid_base, 13)
    assert g1.get() == 13
    assert not g1.has_alias()
    with pytest.raises(TypeError) as excinfo:
        PythFactory7(tag.shared_ptr, tag.invalid_base, 14)
    assert (str(excinfo.value) ==
            "pybind11::init(): construction failed: returned holder-wrapped instance is not an "
            "alias instance")

    assert [i.alive() for i in cstats] == [13, 7]
    assert ConstructorStats.detail_reg_inst() == n_inst + 13

    del a1, a2, b1, d1, e1, e2
    assert [i.alive() for i in cstats] == [7, 4]
    assert ConstructorStats.detail_reg_inst() == n_inst + 7
    del b2, c1, c2, d2, f1, f2, g1
    assert [i.alive() for i in cstats] == [0, 0]
    assert ConstructorStats.detail_reg_inst() == n_inst

    assert [i.values() for i in cstats] == [
        ["1", "2", "3", "4", "5", "6", "7", "8", "9", "100", "11", "12", "13", "14"],
        ["2", "4", "6", "8", "9", "100", "12"]
    ]
