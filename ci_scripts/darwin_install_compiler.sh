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


case "${COMPILER}-${VERSION}" in
gcc-9)
	upgradeBrewFormula gcc@9
	;;
gcc-10)
	upgradeBrewFormula gcc@10
	;;
gcc-11)
	upgradeBrewFormula gcc@11
	;;
gcc-12)
	upgradeBrewFormula gcc@12
	;;
clang-12)
	upgradeBrewFormula llvm@12
	# 12 is keg-only, so we have to set the full file path
	export CC=/usr/local/opt/llvm@12/bin/clang
	export CXX=/usr/local/opt/llvm@12/bin/clang++
	;;
clang-13)
	upgradeBrewFormula llvm@13
	# 13 is keg-only, so we have to set the full file path
	export CC=/usr/local/opt/llvm@13/bin/clang
	export CXX=/usr/local/opt/llvm@13/bin/clang++
	;;
clang-14)
	upgradeBrewFormula llvm@14
	# 14 is keg-only, so we have to set the full file path
	export CC=/usr/local/opt/llvm@14/bin/clang
	export CXX=/usr/local/opt/llvm@14/bin/clang++
	;;
clang-15)
	upgradeBrewFormula llvm@15
	# 15 is keg-only, so we have to set the full file path
	export CC=/usr/local/opt/llvm@15/bin/clang
	export CXX=/usr/local/opt/llvm@15/bin/clang++
	;;
clang-16)
	upgradeBrewFormula llvm@16
	# 16 is keg-only, so we have to set the full file path
	export CC=/usr/local/opt/llvm@16/bin/clang
	export CXX=/usr/local/opt/llvm@16/bin/clang++
	;;
*) echo "Compiler not supported: ${COMPILER}-${VERSION}. See travis_ci_install_osx.sh"; exit 1 ;;
esac
