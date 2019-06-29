#!/bin/bash

clang-format-8 -i `find benchmarks source include tests -type f -iname '*.cpp' -or -iname '*.h'`
exit $?
