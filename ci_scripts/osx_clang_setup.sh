#!/usr/bin/env bash -x

curl http://llvm.org/releases/3.7.0/clang+llvm-3.7.0-x86_64-apple-darwin.tar.xz > clang.tar.xz
tar -xf clang.tar.xz
sudo mkdir /usr/local/clang

sudo cp -R clang+llvm-3.7.0-x86_64-apple-darwin/* /usr/local/clang/

sudo rm /usr/bin/clang
sudo rm /usr/bin/clang++

sudo ln -s /usr/local/clang/bin/clang /usr/bin/clang
sudo ln -s /usr/local/clang/bin/clang++ /usr/bin/clang++

clang++ -v


sudo brew update > /dev/null
brew outdated boost || brew upgrade boost
