#!/bin/bash -x

set -e

brew update

case "${CXX}" in
g++-4.8)     brew outdated gcc48 || brew upgrade gcc48 ;;
g++-4.9)     brew install gcc49 || brew link --overwrite gcc49;;
g++-5)       brew install gcc5 ;;
clang++-3.5) brew install llvm35 --with-clang ;;
clang++-3.6) brew install llvm36 --with-clang ;;
*) echo "Compiler not supported: ${CXX}. See travis_ci_install_osx.sh"; exit 1 ;;
esac

brew outdated boost || brew upgrade boost
