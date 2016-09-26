#!/bin/bash -x

set -e

# Install latest boost
wget https://files.adrianastley.com/programming/boost_1_60_0.tar.bz2
tar --bzip2 -xf boost_1_60_0.tar.bz2

cd boost_1_60_0
./bootstrap.sh toolset=${CXX} --with-libraries=context
./b2

# Get back to the root
cd ../
