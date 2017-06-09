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

// Execution plan

#ifndef NONIUS_EXECUTION_PLAN_HPP
#define NONIUS_EXECUTION_PLAN_HPP

#include <nonius/clock.hpp>
#include <nonius/environment.hpp>
#include <nonius/optimizer.hpp>
#include <nonius/detail/benchmark_function.hpp>
#include <nonius/detail/repeat.hpp>
#include <nonius/detail/run_for_at_least.hpp>

#include <algorithm>

namespace nonius {
    template <typename Duration>
    struct execution_plan {
        int iterations_per_sample = 0;
        Duration estimated_duration = {};
        parameters params;
        detail::benchmark_function benchmark;
        Duration warmup_time = {};
        int warmup_iterations = 0;

        execution_plan() = default;

        execution_plan(int iterations_per_sample, Duration estimated_duration, parameters params, detail::benchmark_function benchmark, Duration warmup_time, int warmup_iterations)
            : iterations_per_sample(iterations_per_sample)
            , estimated_duration(estimated_duration)
            , params(params)
            , benchmark(benchmark)
            , warmup_time(warmup_time)
            , warmup_iterations(warmup_iterations)
        {}

        template <typename Duration2>
        operator execution_plan<Duration2>() const {
            return execution_plan<Duration2>{ iterations_per_sample, estimated_duration, params, benchmark, warmup_time, warmup_iterations };
        }

        template <typename Clock>
        std::vector<FloatDuration<Clock>> run(configuration cfg, environment<FloatDuration<Clock>> env) const {
            // warmup a bit
            detail::run_for_at_least<Clock>(params, chrono::duration_cast<nonius::Duration<Clock>>(warmup_time), warmup_iterations, detail::repeat(now<Clock>{}));

            std::vector<FloatDuration<Clock>> times;
            times.reserve(cfg.samples);
            std::generate_n(std::back_inserter(times), cfg.samples, [this, env]{
                    detail::chronometer_model<Clock> model;
                    detail::optimizer_barrier();
                    this->benchmark(chronometer(model, iterations_per_sample, params));
                    detail::optimizer_barrier();
                    auto sample_time = model.elapsed() - env.clock_cost.mean;
                    if(sample_time < FloatDuration<Clock>::zero()) sample_time = FloatDuration<Clock>::zero();
                    return sample_time / iterations_per_sample;
            });
            return times;
        }
    };
} // namespace nonius

#endif // NONIUS_EXECUTION_PLAN_HPP
