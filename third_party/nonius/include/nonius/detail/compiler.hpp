// Nonius - C++ benchmarking tool
//
// Written in 2014- by the nonius contributors <nonius@rmf.io>
//
// To the extent possible under law, the author(s) have dedicated all copyright and related
// and neighboring rights to this software to the public domain worldwide. This software is
// distributed without any warranty.
//
// You should have received a copy of the CC0 Public Domain Dedication along with this software.
// If not, see <http://creativecommons.org/publicdomain/zero/1.0/>

// Compiler detection macros

#ifndef NONIUS_DETAIL_COMPILER_HPP
#define NONIUS_DETAIL_COMPILER_HPP

#if defined(__clang__)
#   define NONIUS_CLANG (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
#elif defined(__GNUC__)
#   define NONIUS_GCC   (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#elif defined(_MSC_VER)
#   define NONIUS_MSVC  _MSC_VER
#endif

#endif // NONIUS_DETAIL_COMPILER_HPP
