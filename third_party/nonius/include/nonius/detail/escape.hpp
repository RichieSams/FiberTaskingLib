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

// General escaping routines

#ifndef NONIUS_DETAIL_ESCAPE_HPP
#define NONIUS_DETAIL_ESCAPE_HPP

#include <string>
#include <algorithm>
#include <iterator>
#include <utility>

namespace nonius {
    namespace detail {
        inline std::string escape(std::string const& source, std::unordered_map<char, std::string> const& escapes) {
            std::string magic;
            magic.reserve(escapes.size());
            std::transform(escapes.begin(), escapes.end(), std::back_inserter(magic), [](std::pair<char const, std::string> const& p) { return p.first; });

            auto first = source.begin();
            auto last = source.end();

            auto n_magic = std::count_if(first, last, [&magic](char c) { return magic.find(c) != std::string::npos; });

            std::string escaped;
            escaped.reserve(source.size() + n_magic*6);

            while(first != last) {
                auto next_magic = std::find_first_of(first, last, magic.begin(), magic.end());
                std::copy(first, next_magic, std::back_inserter(escaped));
                first = next_magic;
                if(first != last) {
                    auto it = escapes.find(*first);
                    if(it != escapes.end()) {
                        escaped += it->second;
                    }
                    ++first;
                }
            }
            return escaped;
        }
    } // namespace detail
} // namespace nonius

#endif // NONIUS_DETAIL_ESCAPE_HPP

