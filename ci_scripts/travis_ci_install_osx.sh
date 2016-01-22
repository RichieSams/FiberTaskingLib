#!/bin/bash -x

set -e

brew update
brew tap homebrew/versions

install_brew_package() {
  if [[ brew list "$1" ]] ; then
    # Package is installed, upgrade if needed
    brew outdated "$1" || brew upgrade "$1"
  else
    # Package not installed yet, install.
    brew install "$1"
  fi
}

case "${CXX}" in
g++-4.8)     install_brew_package homebrew/versions/gcc48 ;;
g++-4.9)     install_brew_package homebrew/versions/gcc49 ;;
g++-5)       install_brew_package homebrew/versions/gcc5 ;;
clang++-3.5) install_brew_package homebrew/versions/llvm35 --with-clang ;;
clang++-3.6) install_brew_package homebrew/versions/llvm36 --with-clang ;;
*) echo "Compiler not supported: ${CXX}. See travis_ci_install_osx.sh"; exit 1 ;;
esac

install_brew_package boost
