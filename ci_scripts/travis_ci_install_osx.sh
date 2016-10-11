#!/bin/bash
set -x #echo on

brew update

case "${CXX}" in
g++-4.8)
	brew outdated gcc48 || brew upgrade gcc48
	# Force a relink
	brew link --overwrite gcc48
	;;
g++-4.9)
	brew outdated gcc49 || brew upgrade gcc49
	# Force a re-link
	brew link --overwrite gcc49
	;;
g++-5)
	brew outdated gcc5 || brew install gcc5
	;;
clang++-3.5)
	brew outdated llvm35 || brew install llvm35
	;;
clang++-3.6)
	brew outdated llvm36 || brew install llvm36
	;;
clang++-3.7)
	brew outdated llvm37 || brew install llvm37
	;;
*) echo "Compiler not supported: ${CXX}. See travis_ci_install_osx.sh"; exit 1 ;;
esac
