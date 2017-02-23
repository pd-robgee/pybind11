import pytest


def test_multiple_inheritance_cpp():
    from pybind11_tests import MIType

    mt = MIType(3, 4)

    assert mt.foo() == 3
    assert mt.bar() == 4


def test_multiple_inheritance_mix1():
    from pybind11_tests import Base2

    class Base1:
        def __init__(self, i):
            self.i = i

        def foo(self):
            return self.i

    class MITypePy(Base1, Base2):
        def __init__(self, i, j):
            Base1.__init__(self, i)
            Base2.__init__(self, j)

    mt = MITypePy(3, 4)

    assert mt.foo() == 3
    assert mt.bar() == 4


def test_multiple_inheritance_mix2():
    from pybind11_tests import Base1

    class Base2:
        def __init__(self, i):
            self.i = i

        def bar(self):
            return self.i

    class MITypePy(Base1, Base2):
        def __init__(self, i, j):
            Base1.__init__(self, i)
            Base2.__init__(self, j)

    mt = MITypePy(3, 4)

    assert mt.foo() == 3
    assert mt.bar() == 4


def test_multiple_inheritance_python():
    from pybind11_tests import Base1, Base2

    class MI1(Base1, Base2):
        def __init__(self, i, j):
            Base1.__init__(self, i)
            Base2.__init__(self, j)

    class B1(object):
        def v(self):
            return 1

    class MI2(B1, Base1, Base2):
        def __init__(self, i, j):
            B1.__init__(self)
            Base1.__init__(self, i)
            Base2.__init__(self, j)

    class MI3(MI2):
        def __init__(self, i, j):
            MI2.__init__(self, i, j)

    class MI4(MI3, Base2):
        def __init__(self, i, j, k):
            MI2.__init__(self, j, k)
            Base2.__init__(self, i)

    class MI5(Base2, B1, Base1):
        def __init__(self, i, j):
            B1.__init__(self)
            Base1.__init__(self, i)
            Base2.__init__(self, j)

    class MI6(Base2, B1):
        def __init__(self, i):
            Base2.__init__(self, i)
            B1.__init__(self)

    class B2(B1):
        def v(self):
            return 2

    class B3(object):
        def v(self):
            return 3

    class B4(B3, B2):
        def v(self):
            return 4

    class MI7(B4, MI6):
        def __init__(self, i):
            B4.__init__(self)
            MI6.__init__(self, i)

    class MI8(MI6, B3):
        def __init__(self, i):
            MI6.__init__(self, i)
            B3.__init__(self)

    class MI8b(B3, MI6):
        def __init__(self, i):
            B3.__init__(self)
            MI6.__init__(self, i)

    mi1 = MI1(1, 2)
    assert mi1.foo() == 1
    assert mi1.bar() == 2

    mi2 = MI2(3, 4)
    assert mi2.v() == 1
    assert mi2.foo() == 3
    assert mi2.bar() == 4

    mi3 = MI3(5, 6)
    assert mi3.v() == 1
    assert mi3.foo() == 5
    assert mi3.bar() == 6

    mi4 = MI4(7, 8, 9)
    assert mi4.v() == 1
    assert mi4.foo() == 8
    assert mi4.bar() == 7

    mi5 = MI5(10, 11)
    assert mi5.v() == 1
    assert mi5.foo() == 10
    assert mi5.bar() == 11

    mi6 = MI6(12)
    assert mi6.v() == 1
    assert mi6.bar() == 12

    mi7 = MI7(13)
    assert mi7.v() == 4
    assert mi7.bar() == 13

    mi8 = MI8(14)
    assert mi8.v() == 1
    assert mi8.bar() == 14

    mi8b = MI8b(15)
    assert mi8b.v() == 3
    assert mi8b.bar() == 15


def test_multiple_inheritance_virtbase():
    from pybind11_tests import Base12a, bar_base2a, bar_base2a_sharedptr

    class MITypePy(Base12a):
        def __init__(self, i, j):
            Base12a.__init__(self, i, j)

    mt = MITypePy(3, 4)
    assert mt.bar() == 4
    assert bar_base2a(mt) == 4
    assert bar_base2a_sharedptr(mt) == 4


def test_mi_static_properties():
    """Mixing bases with and without static properties should be possible
     and the result should be independent of base definition order"""
    from pybind11_tests import mi

    for d in (mi.VanillaStaticMix1(), mi.VanillaStaticMix2()):
        assert d.vanilla() == "Vanilla"
        assert d.static_func1() == "WithStatic1"
        assert d.static_func2() == "WithStatic2"
        assert d.static_func() == d.__class__.__name__

        mi.WithStatic1.static_value1 = 1
        mi.WithStatic2.static_value2 = 2
        assert d.static_value1 == 1
        assert d.static_value2 == 2
        assert d.static_value == 12

        d.static_value1 = 0
        assert d.static_value1 == 0
        d.static_value2 = 0
        assert d.static_value2 == 0
        d.static_value = 0
        assert d.static_value == 0


@pytest.unsupported_on_pypy
def test_mi_dynamic_attributes():
    """Mixing bases with and without dynamic attribute support"""
    from pybind11_tests import mi

    for d in (mi.VanillaDictMix1(), mi.VanillaDictMix2()):
        d.dynamic = 1
        assert d.dynamic == 1
