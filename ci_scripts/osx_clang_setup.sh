#!/usr/bin/env bash -x

sudo brew update

curl http://llvm.org/releases/3.7.0/clang+llvm-3.7.0-x86_64-apple-darwin.tar.xz > clang+llvm-3.7.tar.xz
tar xzf clang+llvm-3.7.tar.xz
cd clang+llvm-3.7
sudo cp -R * /usr/local/
cd ../

export CXX="clang++-3.7" CC="clang-3.7"

brew outdated boost || brew upgrade boost
