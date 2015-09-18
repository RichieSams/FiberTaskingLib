#!/usr/bin/env bash

sudo brew update

sudo brew tap homebrew/versions
sudo brew install --enable-cxx gcc5
sudo brew install llvm --with-clang --with-asan --HEAD

brew outdated boost || brew upgrade boost > /dev/null
