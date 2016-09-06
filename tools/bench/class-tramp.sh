#!/bin/bash

set -e
. `dirname $0`/class-common.sh

title "binding classes with trampoline"


for CLASSES in ${COUNTS:-100 400 1000}; do
    (
        echo "#include <pybind11/pybind11.h>"
        for ((i = 0; i < CLASSES; i++)); do
            echo "class Class$i {}; class PyClass$i : public Class$i {};"
        done
        echo -e "PYBIND11_PLUGIN(test_$CLASSES) {
    namespace py = pybind11;
    py::module m(\"test_$CLASSES\");"
        for ((i = 0; i < CLASSES; i++)); do
            echo -n "    py::class_<Class$i"
            if [ -n "$master" ]; then echo -n ", std::unique_ptr<Class$i>"; fi
            echo ", PyClass$i>(m, \"Class$i\");";
        done
        echo -e "    return m.ptr();\n}"
    ) >test_$CLASSES.cpp

    compile test_$CLASSES.cpp
done
