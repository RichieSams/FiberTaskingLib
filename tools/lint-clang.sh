#!/bin/bash

rm -rf build_lint
mkdir -p build_lint
(cd build_lint && exec cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ../)
set -x
tools/run-clang-tidy.py -clang-tidy-binary clang-tidy-8 -p build_lint -header-filter=include/ftl/.* -q -warnings-as-errors="*" `find benchmarks source include/ftl tests -type f -name '*.cpp'`
tidy_exit_code=$?
set +x

rm -rf build_lint
exit ${tidy_exit_code}
