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

// repeat algorithm

#ifndef NONIUS_DETAIL_REPEAT_HPP
#define NONIUS_DETAIL_REPEAT_HPP

#include <type_traits>
#include <utility>

namespace nonius {
    namespace detail {
        template <typename Fun>
        struct repeater {
            void operator()(int k) const {
                for(int i = 0; i < k; ++i) {
                    fun();
                }
            }
            Fun fun;
        };
        template <typename Fun>
        repeater<typename std::decay<Fun>::type> repeat(Fun&& fun) {
            return { std::forward<Fun>(fun) };
        }
    } // namespace detail
} // namespace nonius

#endif // NONIUS_DETAIL_REPEAT_HPP

