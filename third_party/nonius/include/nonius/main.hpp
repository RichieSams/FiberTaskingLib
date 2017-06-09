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

// Executable building kit

#ifndef NONIUS_MAIN_HPP
#define NONIUS_MAIN_HPP

#include <nonius/nonius.hpp>
#include <nonius/detail/argparse.hpp>
#include <nonius/detail/string_utils.hpp>

#include <vector>
#include <string>
#include <stdexcept>
#include <exception>
#include <iostream>
#include <iomanip>
#include <utility>

namespace nonius {
    namespace detail {
        template <typename T>
        struct parser;
        template <>
        struct parser<int> {
            static int parse(std::string const& s) { return std::stoi(s); }
        };
        template <>
        struct parser<double> {
            static double parse(std::string const& s) { return std::stod(s); }
        };
        template <>
        struct parser<std::string> {
            static std::string parse(std::string const& s) { return s; }
        };
        template <>
        struct parser<bool> {
            static bool parse(std::string const&) { return true; }
        };
        template <>
        struct parser<param_configuration> {
            static param_configuration parse(std::string const& param) {
                auto v = std::vector<std::string>{};
                split(v, param, is_any_of(":"));
                try {
                    if (v.size() > 0) {
                        auto name = v[0];
                        auto def  = global_param_registry().defaults().at(name);
                        if (v.size() == 2)
                            return {{{name, def.parse(v[1])}}, {}};
                        else if (v.size() == 5) {
                            auto oper  = v[1];
                            auto init  = def.parse(v[2]);
                            auto step  = def.parse(v[3]);
                            std::stringstream ss(v[4]);
                            auto count = size_t();
                            ss >> count;
                            ss.exceptions(std::ios::failbit);
                            return {{}, run_configuration{name, oper, init, step, count}};
                        }
                    }
                }
                catch (std::out_of_range const&) {}
                return {};
            }
        };
        struct assign_fn {
            template <typename T, typename U>
            void operator() (T& dest, U&& src) const { dest = std::forward<U>(src); }
        };
        template <typename T, typename Predicate, typename Assignment=assign_fn>
        void parse(T& variable, detail::arguments& args, std::string const& option, Predicate&& is_valid, Assignment&& assign={}) {
            auto rng = args.equal_range(option);
            for (auto it = rng.first; it != rng.second; ++it) {
                if(it != args.end()) {
                    auto value = parser<T>::parse(it->second);
                    if(is_valid(value)) {
                        std::forward<Assignment>(assign) (variable, std::move(value));
                    } else {
                        throw argument_error();
                    }
                }
            }
        }
        template <typename T>
        void parse(T& variable, detail::arguments& args, std::string const& option) {
            return parse(variable, args, option, [](T const&) { return true; });
        }

        inline detail::option_set const& command_line_options() {
            static detail::option_set the_options {
                detail::option("help", "h", "show this help message"),
                detail::option("samples", "s", "number of samples to collect (default: 100)", "SAMPLES"),
                detail::option("resamples", "rs", "number of resamples for the bootstrap (default: 100000)", "RESAMPLES"),
                detail::option("confidence-interval", "ci", "confidence interval for the bootstrap (between 0 and 1, default: 0.95)", "INTERVAL"),
                detail::option("param", "p", "set a benchmark parameter", "PARAM"),
                detail::option("output", "o", "output file (default: <stdout>)", "FILE"),
                detail::option("reporter", "r", "reporter to use (default: standard)", "REPORTER"),
                detail::option("title", "t", "set report title", "TITLE"),
                detail::option("no-analysis", "A", "perform only measurements; do not perform any analysis"),
                detail::option("filter", "f", "only run benchmarks whose name matches the regular expression pattern", "PATTERN"),
                detail::option("list", "l", "list benchmarks"),
                detail::option("list-params", "lp", "list available parameters"),
                detail::option("list-reporters", "lr", "list available reporters"),
                detail::option("verbose", "v", "show verbose output (mutually exclusive with -q)"),
                detail::option("summary", "q", "show summary output (mutually exclusive with -v)")
            };
            return the_options;
        }

        template <typename Iterator>
        configuration parse_args(std::string const& name, Iterator first, Iterator last) {
            try {
                auto args = detail::parse_arguments(command_line_options(), first, last);

                configuration cfg;

                auto is_positive = [](int x) { return x > 0; };
                auto is_ci = [](double x) { return x > 0 && x < 1; };
                auto is_reporter = [](std::string const x) { return global_reporter_registry().count(x) > 0; };
                auto is_param = [](param_configuration const& x) {
                    return !x.map.empty() || x.run;
                };
                auto merge_params = [](param_configuration& x, param_configuration&& y) {
                    x.map = std::move(x.map).merged(std::move(y.map));
                    if (y.run) x.run = y.run;
                };

                parse(cfg.help, args, "help");
                parse(cfg.samples, args, "samples", is_positive);
                parse(cfg.resamples, args, "resamples", is_positive);
                parse(cfg.confidence_interval, args, "confidence-interval", is_ci);
                parse(cfg.params, args, "param", is_param, merge_params);
                parse(cfg.output_file, args, "output");
                parse(cfg.reporter, args, "reporter", is_reporter);
                parse(cfg.no_analysis, args, "no-analysis");
                parse(cfg.filter_pattern, args, "filter");
                parse(cfg.list_benchmarks, args, "list");
                parse(cfg.list_params, args, "list-params");
                parse(cfg.list_reporters, args, "list-reporters");
                parse(cfg.verbose, args, "verbose");
                parse(cfg.summary, args, "summary");
                parse(cfg.title, args, "title");
                if(cfg.verbose && cfg.summary) throw argument_error();

                return cfg;
            } catch(...) {
                std::cout << help_text(name, command_line_options());
                throw argument_error();
            }
        }
    } // namespace detail

    inline int print_help(std::string const& name) {
        std::cout << detail::help_text(name, detail::command_line_options());
        return 0;
    }
    inline int list_benchmarks() {
        std::cout << "All available benchmarks:\n";
        for(auto&& b : global_benchmark_registry()) {
            std::cout << "  " << b.name << "\n";
        }
        std::cout << global_benchmark_registry().size() << " benchmarks\n\n";
        return 0;
    }
    inline int list_params() {
        std::cout << "Available parameters (= default):\n"
                  << global_param_registry().defaults();
        return 0;
    }
    inline int list_reporters() {
        using reporter_entry_ref = decltype(*global_reporter_registry().begin());
        auto cmp = [](reporter_entry_ref a, reporter_entry_ref b) { return a.first.size() < b.first.size(); };
        auto width = 2 + std::max_element(global_reporter_registry().begin(), global_reporter_registry().end(), cmp)->first.size();

        std::cout << "Available reporters:\n";
        std::cout << std::left;
        for(auto&& r : global_reporter_registry()) {
            if(!r.first.empty()) {
                std::cout << "  " << std::setw(width) << r.first << r.second->description() << "\n";
            }
        }
        std::cout << '\n';
        return 0;
    }
    inline int run_it(configuration cfg) {
        try {
            nonius::go(cfg);
        } catch(...) {
            std::cerr << "PANIC: clock is on fire\n";
            try {
                throw;
            } catch(std::exception const& e) {
                std::cerr << "  " << e.what() << "\n";
            } catch(...) {}
            return 23;
        }
        return 0;
    }

    template <typename Iterator>
    int main(std::string const& name, Iterator first, Iterator last) {
        configuration cfg;

        try {
            cfg = detail::parse_args(name, first, last);
        } catch(detail::argument_error const&) {
            return 17;
        }

        if(cfg.help) return print_help(name);
        else if(cfg.list_benchmarks) return list_benchmarks();
        else if(cfg.list_params) return list_params();
        else if(cfg.list_reporters) return list_reporters();
        else return run_it(cfg);
    }
    inline int main(int argc, char** argv) {
        std::string name(argv[0]);
        std::vector<std::string> args(argv+1, argv+argc);
        return main(name, args.begin(), args.end());
    }
}

#ifdef NONIUS_RUNNER
int main(int argc, char** argv) {
    return nonius::main(argc, argv);
}
#endif // NONIUS_RUNNER

#endif // NONIUS_MAIN_HPP
