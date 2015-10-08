#!/usr/bin/env bash -x

sudo brew update > /dev/null

sudo brew tap homebrew/versions
sudo brew install --enable-cxx gcc5

brew outdated boost || brew upgrade boost
