#!/bin/bash -x

set -e

sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
wget -O - http://llvm.org/apt/llvm-snapshot.gpg.key | sudo apt-key add -
sudo add-apt-repository -y 'deb http://llvm.org/apt/trusty/ llvm-toolchain-trusty main'
sudo add-apt-repository -y 'deb http://llvm.org/apt/trusty/ llvm-toolchain-trusty-3.6 main'
sudo add-apt-repository -y 'deb http://llvm.org/apt/trusty/ llvm-toolchain-trusty-3.7 main'
sudo apt-get update -qq

# Always install latest GCC 4.8 to avoid bugs in old STL when compiling with Clang.
sudo apt-get install -qq --force-yes g++-4.8
sudo apt-get install -qq --force-yes p7zip-full ${CXX}

# Install latest boost
curl https://files.adrianastley.com/programming/boost_1_58_0.tar.7z > boost_1_58_0.tar.7z
7z x boost_1_58_0.tar.7z > /dev/null
tar xf boost_1_58_0.tar
cd boost_1_58_0
./bootstrap.sh toolset=${CXX} --with-libraries=context
sudo ./b2 install > /dev/null

# Get back to the root
cd ../
