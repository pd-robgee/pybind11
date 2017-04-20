import pytest
from pybind11_tests import ConstructorStats


def test_init_factory():
    """Tests py::init_factory() wrapper around various ways of returning the object"""
    from pybind11_tests import TestFactory1, TestFactory2, TestFactory3, tag

    cstats = [ConstructorStats.get(c) for c in [TestFactory1, TestFactory2, TestFactory3]]
    cstats[0].alive()  # force gc
    n_inst = ConstructorStats.detail_reg_inst()

    x1 = TestFactory1(tag.pointer, 3)
    assert x1.value == "3"
    y1 = TestFactory1(tag.pointer)
    assert y1.value == "(empty)"
    z1 = TestFactory1(tag.unique_ptr, "hi!")
    assert z1.value == "hi!"

    def get_test_factory_1(*args):
        v = -23 + sum(args)
        return TestFactory1(tag.pointer, v)
    w1 = TestFactory1(get_test_factory_1)
    assert w1.value == "100"

    TestFactory1.set_ctor_fallback(get_test_factory_1)
    v1 = TestFactory1()
    assert v1.value == "-23"

    assert ConstructorStats.detail_reg_inst() == n_inst + 5

    x2 = TestFactory2(tag.move)
    assert x2.value == "(empty2)"
    y2 = TestFactory2(tag.pointer, 7)
    assert y2.value == "7"
    z2 = TestFactory2(tag.unique_ptr, "hi again")
    assert z2.value == "hi again"

    assert ConstructorStats.detail_reg_inst() == n_inst + 8

    v3 = TestFactory3(tag.object, 8)  # lambda adds 1
    assert v3.value == "9"
    w3 = TestFactory3(tag.raw_object, 99)
    assert w3.value == "99"
    x3 = TestFactory3(tag.shared_ptr)
    assert x3.value == "(empty3)"
    y3 = TestFactory3(tag.pointer, 42)
    assert y3.value == "42"
    z3 = TestFactory3("bye")
    assert z3.value == "bye"

    assert ConstructorStats.detail_reg_inst() == n_inst + 13

    with pytest.raises(TypeError) as excinfo:
        TestFactory3(tag.multiref, 21)
    assert (str(excinfo.value) ==
            "__init__() factory function returned an object with multiple references")
    assert ConstructorStats.detail_reg_inst() == n_inst + 14  # +1 because the above leaks a ref
    with pytest.raises(TypeError) as excinfo:
        TestFactory3(tag.unowned, 24)
    assert (str(excinfo.value) ==
            "__init__() factory function returned an unowned reference")
    assert ConstructorStats.detail_reg_inst() == n_inst + 14
    with pytest.raises(TypeError) as excinfo:
        TestFactory3(tag.null_ptr)
    assert (str(excinfo.value) ==
            "__init__() factory function returned a null pointer")
    assert ConstructorStats.detail_reg_inst() == n_inst + 14

    assert [i.alive() for i in cstats] == [5, 3, 7]
    TestFactory3.cleanup_leaks()
    assert ConstructorStats.detail_reg_inst() == n_inst + 13
    assert [i.alive() for i in cstats] == [5, 3, 5]
    del x1, y2, y3, z3, v1
    assert [i.alive() for i in cstats] == [3, 2, 3]
    assert ConstructorStats.detail_reg_inst() == n_inst + 8
    del x2, x3, w1
    assert [i.alive() for i in cstats] == [2, 1, 2]
    assert ConstructorStats.detail_reg_inst() == n_inst + 5
    del y1, z1, z2
    assert [i.alive() for i in cstats] == [0, 0, 2]
    assert ConstructorStats.detail_reg_inst() == n_inst + 2
    del v3, w3
    assert [i.alive() for i in cstats] == [0, 0, 0]
    assert ConstructorStats.detail_reg_inst() == n_inst

    assert [i.values() for i in cstats] == [
        ["3", "hi!", "100", "-23"],
        ["7", "hi again"],
        ["9", "99", "42", "bye", "21", "24"]
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
    del c
    assert [i.alive() for i in cstats] == [4, 3, 1]
    del b
    assert [i.alive() for i in cstats] == [3, 2, 1]
    del e
    assert [i.alive() for i in cstats] == [2, 1, 1]
    assert ConstructorStats.detail_reg_inst() == n_inst + 2
    del f
    assert [i.alive() for i in cstats] == [1, 0, 1]
    del d
    assert [i.alive() for i in cstats] == [0, 0, 0]
    assert ConstructorStats.detail_reg_inst() == n_inst

    # These return a base class which is *not* actually a TestFactory4 instance:
    with pytest.raises(TypeError) as excinfo:
        TestFactory4(tag.pointer, tag.invalid_base, -2)
    assert (str(excinfo.value) ==
            "__init__() factory failed: could not cast base class pointer")
    with pytest.raises(TypeError) as excinfo:
        TestFactory4(tag.shared_ptr, tag.invalid_base, -3)
    assert (str(excinfo.value) ==
            "__init__() factory failed: could not cast shared base class pointer")

    assert [i.values() for i in cstats] == [
        ["4", "5", "6", "7", "8", "9", "-2", "-3"],
        ["4", "5", "8", "9"],
        ["6", "7", "-2", "-3"]
    ]
    assert [i.alive() for i in cstats] == [0, 0, 0]
    assert ConstructorStats.detail_reg_inst() == n_inst


def test_init_factory_alias():
    """Tests py::init_factory() wrapper with value conversions and alias types"""
    from pybind11_tests import TestFactory6, tag

    cstats = [TestFactory6.get_cstats(), TestFactory6.get_alias_cstats()]
    cstats[0].alive()  # force gc
    n_inst = ConstructorStats.detail_reg_inst()

    a = TestFactory6(1)
    assert a.get() == 1
    assert not a.has_alias()
    assert ConstructorStats.detail_reg_inst() == n_inst + 1
    assert [i.alive() for i in cstats] == [1, 0]
    b = TestFactory6("hi there")
    assert b.get() == 8
    assert b.has_alias()
    assert ConstructorStats.detail_reg_inst() == n_inst + 2
    assert [i.alive() for i in cstats] == [2, 1]
    c = TestFactory6(tag.base, 2)
    assert c.get() == 2
    assert not c.has_alias()
    assert ConstructorStats.detail_reg_inst() == n_inst + 3
    assert [i.alive() for i in cstats] == [3, 1]
    d = TestFactory6(tag.alias, 3)
    assert d.get() == 3
    assert d.has_alias()
    assert ConstructorStats.detail_reg_inst() == n_inst + 4
    assert [i.alive() for i in cstats] == [4, 2]
    e = TestFactory6(tag.alias, tag.pointer, 4)
    assert e.get() == 4
    assert e.has_alias()
    assert ConstructorStats.detail_reg_inst() == n_inst + 5
    assert [i.alive() for i in cstats] == [5, 3]
    f = TestFactory6(tag.base, tag.pointer, 5)
    assert f.get() == 5
    assert not f.has_alias()
    assert ConstructorStats.detail_reg_inst() == n_inst + 6
    assert [i.alive() for i in cstats] == [6, 3]
    g = TestFactory6(tag.base, tag.alias, tag.pointer, 6)
    assert g.get() == 6
    assert g.has_alias()
    assert ConstructorStats.detail_reg_inst() == n_inst + 7
    assert [i.alive() for i in cstats] == [7, 4]

    del a, c, f
    assert [i.alive() for i in cstats] == [4, 4]
    assert ConstructorStats.detail_reg_inst() == n_inst + 4
    del b, g
    assert [i.alive() for i in cstats] == [2, 2]
    assert ConstructorStats.detail_reg_inst() == n_inst + 2
    del d, e
    assert [i.alive() for i in cstats] == [0, 0]
    assert ConstructorStats.detail_reg_inst() == n_inst

    class MyTest(TestFactory6):
        def __init__(self, *args):
            TestFactory6.__init__(self, *args)

        def get(self):
            return -5 + TestFactory6.get(self)

    z = MyTest(123)
    assert z.get() == 118
    assert z.has_alias()
    y = MyTest("why hello!")
    assert y.get() == 5
    assert y.has_alias()
    assert ConstructorStats.detail_reg_inst() == n_inst + 2

    alias_failure = ("__init__() factory failed: cannot construct required alias class "
                     "from factory return value")
    with pytest.raises(TypeError) as excinfo:
        MyTest(tag.base, -7)
    assert str(excinfo.value) == alias_failure
    del excinfo
    assert [i.alive() for i in cstats] == [2, 2]
    assert ConstructorStats.detail_reg_inst() == n_inst + 2

    with pytest.raises(TypeError) as excinfo:
        MyTest(tag.unaliasable)
    assert str(excinfo.value) == alias_failure
    del excinfo
    assert [i.alive() for i in cstats] == [2, 2]
    assert ConstructorStats.detail_reg_inst() == n_inst + 2

    del z, y
    assert [i.alive() for i in cstats] == [0, 0]
    assert ConstructorStats.detail_reg_inst() == n_inst

    assert [i.values() for i in cstats] == [
        ["1", "8", "2", "3", "4", "5", "6", "123", "10", "-7"],
        ["hi there", "3", "4", "6", "123", "why hello!"]
    ]
