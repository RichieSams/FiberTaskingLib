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

// Runner entry point

#ifndef NONIUS_GO_HPP
#define NONIUS_GO_HPP

#include <nonius/clock.hpp>
#include <nonius/benchmark.hpp>
#include <nonius/configuration.hpp>
#include <nonius/environment.hpp>
#include <nonius/reporter.hpp>
#include <nonius/reporters/standard_reporter.hpp>
#include <nonius/detail/estimate_clock.hpp>
#include <nonius/detail/analyse.hpp>
#include <nonius/detail/complete_invoke.hpp>
#include <nonius/detail/noexcept.hpp>

#include <algorithm>
#include <set>
#include <exception>
#include <utility>
#include <regex>

namespace nonius {
    namespace detail {
        template <typename Clock>
        environment<FloatDuration<Clock>> measure_environment(reporter& rep) {
            rep.warmup_start();
            auto iters = detail::warmup<Clock>();
            rep.warmup_end(iters);

            rep.estimate_clock_resolution_start();
            auto resolution = detail::estimate_clock_resolution<Clock>(iters);
            rep.estimate_clock_resolution_complete(resolution);

            rep.estimate_clock_cost_start();
            auto cost = detail::estimate_clock_cost<Clock>(resolution.mean);
            rep.estimate_clock_cost_complete(cost);

            return { resolution, cost };
        }
    } // namespace detail

    struct benchmark_user_error : virtual std::exception {
        char const* what() const NONIUS_NOEXCEPT override {
            return "a benchmark failed to run successfully";
        }
    };

    template <typename Fun>
    detail::CompleteType<detail::ResultOf<Fun()>> user_code(reporter& rep, Fun&& fun) {
        try {
            return detail::complete_invoke(std::forward<Fun>(fun));
        } catch(...) {
            rep.benchmark_failure(std::current_exception());
            throw benchmark_user_error();
        }
    }

    inline std::vector<parameters> generate_params(param_configuration cfg) {
        auto params = global_param_registry().defaults().merged(cfg.map);
        if (!cfg.run) {
            return {params};
        } else {
            using operation_t = std::function<param(param, param)>;

            auto&& run = *cfg.run;
            auto step = run.step;
            auto oper = std::unordered_map<std::string, operation_t> {
                {"+", std::plus<param>{}},
                {"*", std::multiplies<param>{}},
            }.at(run.op);

            auto r = std::vector<parameters>{};
            auto next = run.init;
            std::generate_n(std::back_inserter(r), std::max(run.count, std::size_t{1}), [&] {
                auto last = next;
                next = oper(next, step);
                return params.merged(parameters{{run.name, last}});
            });

            return r;
        }
    }

    template <typename Iterator>
    std::vector<benchmark> filter_benchmarks(Iterator first, Iterator last, std::string pattern) {
        auto r = std::vector<benchmark>{};
        auto matcher = std::regex{pattern};
        std::copy_if(first, last, std::back_inserter(r), [&] (benchmark const& b) {
            return std::regex_match(b.name, matcher);
        });
        return r;
    }

    template <typename Clock = default_clock, typename Iterator>
    void go(configuration cfg, Iterator first, Iterator last, reporter& rep) {
        rep.configure(cfg);

        auto env = detail::measure_environment<Clock>(rep);
        rep.suite_start();

        auto benchmarks = filter_benchmarks(first, last, cfg.filter_pattern);
        auto all_params = generate_params(cfg.params);

        for (auto&& params : all_params) {
            rep.params_start(params);
            for (auto&& bench : benchmarks) {
                rep.benchmark_start(bench.name);

                auto plan = user_code(rep, [&]{
                    return bench.template prepare<Clock>(cfg, params, env);
                });

                rep.measurement_start(plan);
                auto samples = user_code(rep, [&]{
                    return plan.template run<Clock>(cfg, env);
                });
                rep.measurement_complete(std::vector<fp_seconds>(samples.begin(), samples.end()));

                if(!cfg.no_analysis) {
                    rep.analysis_start();
                    auto analysis = detail::analyse(cfg, env, samples.begin(), samples.end());
                    rep.analysis_complete(analysis);
                }

                rep.benchmark_complete();
            }
            rep.params_complete();
        }

        rep.suite_complete();
    }
    struct duplicate_benchmarks : virtual std::exception {
        char const* what() const NONIUS_NOEXCEPT override {
            return "two or more benchmarks with the same name were registered";
        }
    };
    template <typename Clock = default_clock, typename Iterator>
    void validate_benchmarks(Iterator first, Iterator last) {
        struct strings_lt_through_pointer {
            bool operator()(std::string* a, std::string* b) const { return *a < *b; };
        };
        std::set<std::string*, strings_lt_through_pointer> names;
        for(; first != last; ++first) {
            if(!names.insert(&first->name).second)
                throw duplicate_benchmarks();
        }
    }
    template <typename Clock = default_clock, typename Iterator>
    void go(configuration cfg, Iterator first, Iterator last, reporter&& rep) {
        go<Clock>(cfg, first, last, rep);
    }
    struct no_such_reporter : virtual std::exception {
        char const* what() const NONIUS_NOEXCEPT override {
            return "reporter could not be found";
        }
    };
    template <typename Clock = default_clock>
    void go(configuration cfg, benchmark_registry& benchmarks = global_benchmark_registry(), reporter_registry& reporters = global_reporter_registry()) {
        auto it = reporters.find(cfg.reporter);
        if(it == reporters.end()) throw no_such_reporter();
        validate_benchmarks<Clock>(benchmarks.begin(), benchmarks.end());
        go<Clock>(cfg, benchmarks.begin(), benchmarks.end(), *it->second);
    }
} // namespace nonius

#endif // NONIUS_GO_HPP
