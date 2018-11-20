project(pmr CXX)

cmake_minimum_required(VERSION 3.2)

add_library(pmr STATIC src/polymorphic_allocator.cpp include/polymorphic_allocator.h include/pmr_vector.h)
target_include_directories(pmr PUBLIC include)
