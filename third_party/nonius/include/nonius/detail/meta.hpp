// Nonius - C++ benchmarking tool
//
// Written in 2014 by Martinho Fernandes <martinho.fernandes@gmail.com>
//
// To the extent possible under law, the author(s) have dedicated all copyright and related
// and neighboring rights to this software to the public domain worldwide. This software is
// distributed without any warranty.
//
// You should have received a copy of the CC0 Public Domain Dedication along with this software.
// If not, see <http://creativecommons.org/publicdomain/zero/1.0/>

// Invoke with a special case for void

#ifndef NONIUS_META_HPP
#define NONIUS_META_HPP

#include <type_traits>
#include <utility>

namespace nonius {
    namespace detail {
        template <typename> struct true_given : std::true_type {};
        struct is_callable_tester {
            template <typename Fun, typename... Args>
            true_given<decltype(std::declval<Fun>()(std::declval<Args>()...))> static test(int);
            template <typename...>
            std::false_type static test(...);
        };
        template <typename T>
        struct is_callable;
        template <typename Fun, typename... Args>
        struct is_callable<Fun(Args...)> : decltype(is_callable_tester::test<Fun, Args...>(0)) {};
    } // namespace detail
} // namespace nonius

#endif // NONIUS_META_HPP
