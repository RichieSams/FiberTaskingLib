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

// Command-line argument parsing

#ifndef NONIUS_ARGPARSE_HPP
#define NONIUS_ARGPARSE_HPP

#include <nonius/detail/mismatch.hpp>

#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <ostream>
#include <iomanip>
#include <tuple>
#include <functional>

namespace nonius {
    namespace detail {
        struct option {
            bool matches_short(std::string const& s) const {
                return s == ("-" + short_form);
            }
            std::tuple<bool, std::string::const_iterator> long_separator(std::string const& s) const {
                auto l = "--" + long_form;
                auto its = detail::mismatch(s.begin(), s.end(), l.begin(), l.end(), [](char a, char b) { return a == b; });
                return std::make_tuple(its.second == l.end(), its.first);
            }
            bool matches_long(std::string const& s) const {
                return std::get<0>(long_separator(s));
            }
            bool matches_long(std::string const& s, std::string& argument_out) const {
                bool match; std::string::const_iterator it;
                std::tie(match, it) = long_separator(s);
                if(match && it != s.end()) {
                    if(*it == '=') argument_out.assign(it+1, s.end());
                    else return false;
                }
                return match;
            }

            option(std::string long_form, std::string short_form, std::string description, std::string argument = std::string(), bool multiple = false)
            : long_form(std::move(long_form)), short_form(std::move(short_form)), description(std::move(description)), argument(std::move(argument)), multiple(multiple) {}

            std::string long_form;
            std::string short_form;
            std::string description;
            std::string argument;
            bool multiple;
        };

        using option_set = std::vector<option>;

        struct help_text {
            help_text(std::string name, option_set const& options) : name(std::move(name)), options(options) {}
            std::string name;
            option_set const& options;
        };

        template <typename Iterator, typename Projection>
        int get_max_width(Iterator first, Iterator last, Projection proj) {
            auto it = std::max_element(first, last, [&proj](option const& a, option const& b) { return proj(a) < proj(b); });
            return static_cast<int>(proj(*it));
        }

        inline std::ostream& operator<<(std::ostream& os, help_text h) {
            os << "Usage: " << h.name << " [OPTIONS]\n\n";

            auto lwidth = 2 + get_max_width(h.options.begin(), h.options.end(), [](option const& o) { return 2 + o.long_form.size() + 1 + o.argument.size(); });
            auto swidth = 2 + get_max_width(h.options.begin(), h.options.end(), [](option const& o) { return 1 + o.short_form.size() + 1 + o.argument.size(); });

            os << std::left;
            for(auto o : h.options) {
                auto l = "--" + o.long_form;
                auto s = "-" + o.short_form;
                if(!o.argument.empty()) {
                    l += "=" + o.argument;
                    s += " " + o.argument;
                }
                os << std::setw(lwidth) << l
                   << std::setw(swidth) << s
                   << o.description
                   << '\n';
            }
            return os;
        }

        using arguments = std::unordered_multimap<std::string, std::string>;

        struct argument_error {
            virtual ~argument_error() = default;
        };

        template <typename Iterator>
        void parse_short(option const& o, arguments& args, Iterator& first, Iterator last) {
            if(!o.argument.empty()) {
                if(++first != last) {
                    args.emplace(o.long_form, *first);
                } else {
                    throw argument_error();
                }
            } else {
                args.emplace(o.long_form, "");
            }
        }
        inline void parse_long(option const& o, arguments& args, std::string&& arg) {
            args.emplace(o.long_form, std::move(arg));
        }

        template <typename Iterator>
        arguments parse_arguments(option_set const& options, Iterator first, Iterator last) {
            arguments args;
            for(; first != last; ++first) {
                bool parsed = false;
                for(auto&& option : options) {
                    if(option.matches_short(*first)) {
                        parse_short(option, args, first, last);
                        parsed = true;
                    } else {
                        std::string arg;
                        if(option.matches_long(*first, arg)) {
                            parse_long(option, args, std::move(arg));
                            parsed = true;
                        }
                    }
                }
                if(!parsed) {
                    throw argument_error();
                }
            }
            return args;
        }
    } // namespace detail
} // namespace nonius

#endif // NONIUS_ARGPARSE_HPP
