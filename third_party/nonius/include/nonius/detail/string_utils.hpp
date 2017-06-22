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

// String processing utilities

#ifndef NONIUS_STRING_UTILS_HPP
#define NONIUS_STRING_UTILS_HPP

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace nonius {
    namespace detail {
    struct is_spaceF {
        bool operator()(const char c) const {
            return std::isspace(c) != 0;
        }
    };

    struct is_any_ofF {
        std::string chars;
        is_any_ofF(std::string chars) : chars(chars) {}
        bool operator()(const char c) const {
            return chars.find_first_of(c) != std::string::npos;
        }
    };
    } // namespace detail

    inline detail::is_spaceF is_space() { return detail::is_spaceF {}; }

    inline detail::is_any_ofF is_any_of(const char* chars) { return detail::is_any_ofF{chars}; }

    inline bool starts_with(std::string const& input, std::string const& test) {
        if (test.size() <= input.size()) {
            return std::equal(test.begin(), test.end(), input.begin());
        }
        return false;
    }

    template <typename PredicateT>
    std::string trim_copy_if(std::string const& input, PredicateT predicate) {
        const auto begin = std::find_if_not(input.begin(), input.end(), predicate);
        const auto end = std::find_if_not(input.crbegin(), input.crend(), predicate);
        return std::string(begin, end.base());
    }

    inline std::string trim_copy(std::string const& input) {
        return trim_copy_if(input, is_space());
    }

    template <typename PredicateT>
    std::vector<std::string>& split(std::vector<std::string>& result, std::string const& input, PredicateT predicate) {
        std::vector<std::string> tmp;

        const auto end = input.end();
        auto itr = input.begin();

        for (;;) {
            auto sep = std::find_if(itr, end, predicate);
            if (sep == end) {
                break;
            }
            tmp.emplace_back(itr, sep);
            itr = std::find_if_not(sep, end, predicate);
        }
        tmp.emplace_back(itr, end);
        std::swap(result, tmp);

        return result;
    }

} // namespace nonius

#endif // NONIUS_STRING_UTILS_HPP

