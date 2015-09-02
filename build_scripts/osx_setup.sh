#!/usr/bin/env bash

sudo brew update

sudo brew tap homebrew/versions
sudo brew install --enable-cxx gcc5

brew outdated boost || brew upgrade boost
