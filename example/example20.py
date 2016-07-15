#!/usr/bin/env python
from __future__ import print_function
import sys
sys.path.append('.')

from example import Ex20_A
from example import Ex20_B
from example import Ex20_C
from example import Ex20_D
from example import Ex20_F
from example import Ex20_G1
from example import Ex20_G2
from example import Ex20_G3
from example import Ex20_G4
from example import Ex20_H4
from example import print_h1
from example import increment_h2
from example import print_double
from example import print_long
from example import print_string
from example import print_ex20e
from example import print_ex20f

# Ex20_A is declared cpp convertible to double; Ex20_B is a registered subclass of Ex20_A,
# and Ex20_C is a registered subclass of Ex20_B.  All should be convertible to double
# through Ex20_A's base class convertibility.
print_double(Ex20_A()) # 42
print_double(Ex20_A((5 ** (1/2.0) + 1) / 2)) # Phi = 1.6180339...
print_double(Ex20_B()) # 42 (via Ex20_A's conversion operator)
print_double(Ex20_C()) # pi (overridden from A's double conv op)
print_string(Ex20_C()) # the string "pi"
print_double(Ex20_D()) # e (overridden from A's double conv op)
print_string(Ex20_D()) # "e"

try:
    print_ex20e(Ex20_A())
    print("BAD: Ex20_A should not be implicitly convertible to Ex20_E")
except TypeError:
    pass

print_ex20e(Ex20_B()) # 84
print_ex20e(Ex20_C()) # 6.28319 (2*pi)
print_ex20e(Ex20_D()) # 8.15485 (3*e)

print_ex20f(Ex20_F()) # 99
print_ex20f(Ex20_A(0.25)) # 250, via C++ implicit conversion
print_ex20f(Ex20_C()) # 1000pi = 3141.59, via C++ implicit conversion
try:
    print_ex20f(Ex20_F(Ex20_A(4)))
    print("BAD: Ex20_F conversion constructor (from Ex20_A) should not have been exposed to python")
except TypeError:
    pass

print_long(Ex20_G1())
print_long(Ex20_G2())
print_long(Ex20_G3())
print_long(Ex20_G4())

h4 = Ex20_H4(17)
print_h1(h4)
increment_h2(h4)
increment_h2(h4)
increment_h2(h4)
print_h1(h4)
