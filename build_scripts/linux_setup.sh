#!/usr/bin/env bash

sudo add-apt-repository ppa:ubuntu-toolchain-r/test -y
sudo apt-get -qq update
sudo apt-get -qq install gcc-5 g++-5 p7zip-full -y

# Install clang
sudo add-apt-repository 'deb http://llvm.org/apt/precise/ llvm-toolchain-precise-3.7 main'
sudo add-apt-repository 'deb-src http://llvm.org/apt/precise/ llvm-toolchain-precise-3.7 main'
wget -O - http://llvm.org/apt/llvm-snapshot.gpg.key|sudo apt-key add -
sudo apt-get -qq update
sudo apt-get install clang-3.7 lldb-3.7

# Install latest boost
curl https://files.adrianastley.com/programming/boost_1_58_0.tar.7z > boost_1_58_0.tar.7z
7z x boost_1_58_0.tar.7z > /dev/null
tar xf boost_1_58_0.tar
cd boost_1_58_0
./bootstrap.sh --with-libraries=context
sudo ./b2 install > /dev/null
# Get back to the root
cd ../

