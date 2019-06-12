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
gcc-4.9)
	upgradeBrewFormula gcc@4.9
	;;
gcc-5)
	upgradeBrewFormula gcc@5
	;;
gcc-6)
	upgradeBrewFormula gcc@6
	;;
gcc-7)
	upgradeBrewFormula gcc@7
	;;
gcc-8)
	upgradeBrewFormula gcc@7
	;;
clang-3.7)
	upgradeBrewFormula llvm@3.7
	;;
clang-3.8)
	upgradeBrewFormula llvm@3.8
	;;
clang-3.9)
	upgradeBrewFormula llvm@3.9
	# 3.9 is keg-only, so we have to set the full file path
	export CC=/usr/local/opt/llvm@3.9/bin/clang
	export CXX=/usr/local/opt/llvm@3.9/bin/clang++
	;;
clang-4)
	upgradeBrewFormula llvm@4
	# 3.9 is keg-only, so we have to set the full file path
	export CC=/usr/local/opt/llvm@3.9/bin/clang
	export CXX=/usr/local/opt/llvm@3.9/bin/clang++
	;;
clang-5)
	upgradeBrewFormula llvm@5
	# 3.9 is keg-only, so we have to set the full file path
	export CC=/usr/local/opt/llvm@3.9/bin/clang
	export CXX=/usr/local/opt/llvm@3.9/bin/clang++
	;;
clang-6)
	upgradeBrewFormula llvm@6
	# 3.9 is keg-only, so we have to set the full file path
	export CC=/usr/local/opt/llvm@3.9/bin/clang
	export CXX=/usr/local/opt/llvm@3.9/bin/clang++
	;;
clang-7)
	upgradeBrewFormula llvm@7
	# 3.9 is keg-only, so we have to set the full file path
	export CC=/usr/local/opt/llvm@3.9/bin/clang
	export CXX=/usr/local/opt/llvm@3.9/bin/clang++
	;;
*) echo "Compiler not supported: ${COMPILER}-${VERSION}. See travis_ci_install_osx.sh"; exit 1 ;;
esac
