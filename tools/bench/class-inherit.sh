#!/bin/bash

set -e
. `dirname $0`/class-common.sh

title "binding of parent/child class pairs"


# Cut defaults in half here vs other scripts, because for each of these, we bind both a parent and child class:
for CLASSES in ${COUNTS:-50 200 500}; do
    (
        echo "#include <pybind11/pybind11.h>"
        for ((i = 0; i < CLASSES; i++)); do
            echo "class BaseClass$i {}; class Class$i : public BaseClass$i {};"
        done
        echo -e "PYBIND11_PLUGIN(test_$CLASSES) {
    namespace py = pybind11;
    py::module m(\"test_$CLASSES\");"
        for ((i = 0; i < CLASSES; i++)); do
            echo "    py::class_<BaseClass$i>(m, \"B$i\");"
            echo -n "    py::class_<Class$i"
            if [ -n "$master" ]; then
                echo ">(m, \"C$i\", py::base<BaseClass$i>());"
            else
                echo ", BaseClass$i>(m, \"C$i\");"
            fi
        done
        echo -e "    return m.ptr();\n}"
    ) >test_$CLASSES.cpp

    compile test_$CLASSES.cpp
done
