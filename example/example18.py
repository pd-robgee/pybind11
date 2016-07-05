#!/usr/bin/env python
from __future__ import print_function
import sys
sys.path.append('.')

from example import Ex18_A
from example import Ex18_B
from example import Ex18_C
from example import Ex18_D
from example import Ex18_F
from example import print_double
from example import print_string
from example import print_ex18e
from example import print_ex18f

# Ex18_A is declared cpp convertible to double; Ex18_B is a registered subclass of Ex18_A,
# and Ex18_C is a registered subclass of Ex18_B.  All should be convertible to double
# through Ex18_A's base class convertibility.
print_double(Ex18_A()) # 42
print_double(Ex18_A((5 ** (1/2.0) + 1) / 2)) # Phi = 1.6180339...
print_double(Ex18_B()) # 42 (via Ex18_A's conversion operator)
print_double(Ex18_C()) # pi (overridden from A's double conv op)
print_string(Ex18_C()) # the string "pi"
print_double(Ex18_D()) # e (overridden from A's double conv op)
print_string(Ex18_D()) # "e"

try:
    print_ex18e(Ex18_A())
    print("BAD: Ex18_A should not be implicitly convertible to Ex18_E")
except TypeError:
    pass

print_ex18e(Ex18_B()) # 84
print_ex18e(Ex18_C()) # 6.28319 (2*pi)
print_ex18e(Ex18_D()) # 8.15485 (3*e)

print_ex18f(Ex18_F()) # 99
print_ex18f(Ex18_A(0.25)) # 250, via C++ implicit conversion
print_ex18f(Ex18_C()) # 1000pi = 3141.59, via C++ implicit conversion
try:
    print_ex18f(Ex18_F(Ex18_A(4)))
    print("BAD: Ex18_F conversion constructor (from Ex18_A) should not have been exposed to python")
except TypeError:
    pass
