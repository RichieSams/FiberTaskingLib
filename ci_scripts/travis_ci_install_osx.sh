#!/bin/bash -x

set -e

brew update

case "${CXX}" in
g++-4.8)     brew outdated homebrew/versions/gcc48 || brew upgrade homebrew/versions/gcc48 ;;
g++-4.9)     brew install homebrew/versions/gcc49 || brew link --overwrite homebrew/versions/gcc49 ;;
g++-5)       brew install homebrew/versions/gcc5 ;;
clang++-3.5) brew install homebrew/versions/llvm35 ;;
clang++-3.6) brew install homebrew/versions/llvm36;;
clang++-3.7) brew install homebrew/versions/llvm37;;
*) echo "Compiler not supported: ${CXX}. See travis_ci_install_osx.sh"; exit 1 ;;
esac
