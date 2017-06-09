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

// Benchmark

#ifndef NONIUS_BENCHMARK_HPP
#define NONIUS_BENCHMARK_HPP

#include <nonius/clock.hpp>
#include <nonius/configuration.hpp>
#include <nonius/environment.hpp>
#include <nonius/execution_plan.hpp>
#include <nonius/chronometer.hpp>
#include <nonius/param.hpp>
#include <nonius/optimizer.hpp>
#include <nonius/detail/benchmark_function.hpp>
#include <nonius/detail/repeat.hpp>
#include <nonius/detail/run_for_at_least.hpp>
#include <nonius/detail/unique_name.hpp>

#include <algorithm>
#include <functional>
#include <string>
#include <vector>
#include <cmath>

namespace nonius {
    namespace detail {
        const auto warmup_iterations = 10000;
        const auto warmup_time = chrono::milliseconds(100);
        const auto minimum_ticks = 1000;
    } // namespace detail

    struct benchmark {
        benchmark(std::string name, detail::benchmark_function fun)
        : name(std::move(name)), fun(std::move(fun)) {}

        template <typename Clock>
        execution_plan<FloatDuration<Clock>> prepare(configuration cfg, parameters params, environment<FloatDuration<Clock>> env) const {
            auto bench = fun(params);
            auto min_time = env.clock_resolution.mean * detail::minimum_ticks;
            auto run_time = std::max(min_time, chrono::duration_cast<decltype(min_time)>(detail::warmup_time));
            auto&& test = detail::run_for_at_least<Clock>(params, chrono::duration_cast<Duration<Clock>>(run_time), 1, bench);
            int new_iters = static_cast<int>(std::ceil(min_time * test.iterations / test.elapsed));
            return { new_iters, test.elapsed / test.iterations * new_iters * cfg.samples, params, bench, chrono::duration_cast<FloatDuration<Clock>>(detail::warmup_time), detail::warmup_iterations };
        }

        std::string name;
        detail::benchmark_function fun;
    };

    using benchmark_registry = std::vector<benchmark>;

    inline benchmark_registry& global_benchmark_registry() {
        static benchmark_registry registry;
        return registry;
    }

    struct benchmark_registrar {
        template <typename Fun>
        benchmark_registrar(benchmark_registry& registry, std::string name, Fun&& registrant) {
            registry.emplace_back(std::move(name), std::forward<Fun>(registrant));
        }
    };
} // namespace nonius

#define NONIUS_BENCHMARK(name, ...) \
    namespace { static ::nonius::benchmark_registrar NONIUS_DETAIL_UNIQUE_NAME(benchmark_registrar) (::nonius::global_benchmark_registry(), name, __VA_ARGS__); }

#endif // NONIUS_BENCHMARK_HPP
