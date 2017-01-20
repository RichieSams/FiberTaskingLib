#!/bin/bash
set -x #echo on

brew update

upgradeBrewFormula () {
	if brew ls --versions $1 > /dev/null
	then
		brew outdated $1 || brew upgrade $1
	else
		brew install $1
	fi
}


case "${CXX}" in
g++-4.8)
	upgradeBrewFormula gcc48
	;;
g++-4.9)
	upgradeBrewFormula gcc49
	;;
g++-5)
	upgradeBrewFormula gcc5
	;;
clang++-3.7)
	upgradeBrewFormula llvm37
	;;
clang++-3.8)
	upgradeBrewFormula llvm38
	;;
*) echo "Compiler not supported: ${CXX}. See travis_ci_install_osx.sh"; exit 1 ;;
esac
