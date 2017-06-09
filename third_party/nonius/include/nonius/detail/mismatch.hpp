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

// mismatch algorithm

#ifndef NONIUS_DETAIL_MISMATCH_HPP
#define NONIUS_DETAIL_MISMATCH_HPP

#include <utility>

namespace nonius {
    namespace detail {
        template <typename InputIt1, typename InputIt2, typename BinaryPredicate>
        std::pair<InputIt1, InputIt2> mismatch(InputIt1 first1, InputIt1 last1, InputIt2 first2, InputIt2 last2, BinaryPredicate p) {
            while(first1 != last1 && first2 != last2 && p(*first1, *first2)) {
                ++first1, ++first2;
            }
            return std::make_pair(first1, first2);
        }
    } // namespace detail
} // namespace nonius

#endif // NONIUS_DETAIL_MISMATCH_HPP

