cmake_minimum_required(VERSION 3.8)
project(Catch2 CXX)

add_library(catch2 INTERFACE)
target_include_directories(catch2 INTERFACE include)
target_compile_features(catch2 INTERFACE cxx_std_11)
