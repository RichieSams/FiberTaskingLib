#!/bin/bash
set -x #echo on

brew update
brew cask uninstall oclint

upgradeBrewFormula () {
	if brew ls --versions $1 > /dev/null
	then
		brew outdated $1 || brew upgrade $1
	else
		brew install $1
	fi
}


case "${CXX}" in
g++-4.9)
	upgradeBrewFormula gcc@4.9
	;;
g++-5)
	upgradeBrewFormula gcc@5
	;;
g++-6)
	upgradeBrewFormula gcc@6
	;;
clang++-3.7)
	upgradeBrewFormula llvm@3.7
	;;
clang++-3.8)
	upgradeBrewFormula llvm@3.8
	;;
clang++-3.9)
	upgradeBrewFormula llvm@3.9
	# 3.9 is keg-only, so we have to set the full file path
	export MY_CC=/usr/local/opt/llvm@3.9/bin/clang
	export MY_CXX=/usr/local/opt/llvm@3.9/bin/clang++
	;;
*) echo "Compiler not supported: ${CXX}. See travis_ci_install_osx.sh"; exit 1 ;;
esac
