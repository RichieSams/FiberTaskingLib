#!/usr/bin/env bash -x

sudo brew update

sudo brew tap homebrew/versions
sudo brew install llvm --with-clang

clang --version

export CXX="clang++-3.6" CC="clang-3.6"

brew outdated boost || brew upgrade boost
