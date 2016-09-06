#!/bin/bash

set -e
. `dirname $0`/class-common.sh

title "binding classes with shared_ptr holder"


for CLASSES in ${COUNTS:-100 400 1000}; do
    (
        echo "#include <pybind11/pybind11.h>"
        echo "PYBIND11_DECLARE_HOLDER_TYPE(T, std::shared_ptr<T>);"
        for ((i = 0; i < CLASSES; i++)); do
            echo "class Class$i {};"
        done
        echo -e "PYBIND11_PLUGIN(test_$CLASSES) {
    namespace py = pybind11;
    py::module m(\"test_$CLASSES\");"
        for ((i = 0; i < CLASSES; i++)); do
            echo "    py::class_<Class$i, std::shared_ptr<Class$i>>(m, \"Class$i\");";
        done
        echo -e "    return m.ptr();\n}"
    ) >test_$CLASSES.cpp

    compile test_$CLASSES.cpp
done
