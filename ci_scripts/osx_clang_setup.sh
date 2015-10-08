#!/usr/bin/env bash -x

sudo brew update > /dev/null

curl http://llvm.org/releases/3.7.0/clang+llvm-3.7.0-x86_64-apple-darwin.tar.xz > clang+llvm-3.7.0-x86_64-apple-darwin.tar.xz
sudo tar xzf clang*
ls -l
cd clang*
sudo cp -R * /usr/local/
cd ../

brew outdated boost || brew upgrade boost
