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

// Cross-compiler noexcept support

#ifndef NONIUS_DETAIL_NOEXCEPT_HPP
#define NONIUS_DETAIL_NOEXCEPT_HPP

#include <nonius/detail/compiler.hpp>

#if defined(NONIUS_MSVC) && NONIUS_MSVC < 1900
#   define NONIUS_NOEXCEPT
#else
#   define NONIUS_NOEXCEPT noexcept
#endif

#endif // NONIUS_DETAIL_NOEXCEPT_HPP
