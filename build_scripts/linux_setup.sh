#!/usr/bin/env bash

sudo add-apt-repository ppa:ubuntu-toolchain-r/test -y
sudo apt-get -qq update
sudo apt-get -qq install gcc-5 g++-5 p7zip-full -y

# Install clang
sudo bash -c "cat <<EOF > /etc/apt/sources.list
deb http://llvm.org/apt/precise/ llvm-toolchain-precise-3.6 main
deb-src http://llvm.org/apt/precise/ llvm-toolchain-precise-3.6 main
EOF"
wget -O - http://llvm.org/apt/llvm-snapshot.gpg.key|sudo apt-key add -
sudo apt-get update
sudo apt-get install clang-3.6 lldb-3.6



# Install latest boost
curl https://files.adrianastley.com/programming/boost_1_58_0.tar.7z > boost_1_58_0.tar.7z
7z x boost_1_58_0.tar.7z > /dev/null
tar xf boost_1_58_0.tar
cd boost_1_58_0
./bootstrap.sh --with-libraries=context
sudo ./b2 install > /dev/null
# Get back to the root
cd ../

