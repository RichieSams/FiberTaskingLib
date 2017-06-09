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

// Unique name generator macro

#ifndef NONIUS_DETAIL_UNIQUE_NAME_HPP
#define NONIUS_DETAIL_UNIQUE_NAME_HPP

#define NONIUS_DETAIL_UNIQUE_NAME_LINE_CAT(name, id) NONIUS_ ## name ## _ ## id
#define NONIUS_DETAIL_UNIQUE_NAME_LINE(name, id) NONIUS_DETAIL_UNIQUE_NAME_LINE_CAT(name, id)
#ifdef __COUNTER__
#define NONIUS_DETAIL_UNIQUE_NAME(name) NONIUS_DETAIL_UNIQUE_NAME_LINE(name, __COUNTER__)
#else // __COUNTER__
#define NONIUS_DETAIL_UNIQUE_NAME(name) NONIUS_DETAIL_UNIQUE_NAME_LINE(name, __LINE__)
#endif // __COUNTER__

#endif // NONIUS_DETAIL_UNIQUE_NAME_HPP
