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

// Environment measurement

#ifndef NONIUS_DETAIL_ENVIRONMENT_HPP
#define NONIUS_DETAIL_ENVIRONMENT_HPP

#include <nonius/clock.hpp>
#include <nonius/environment.hpp>
#include <nonius/benchmark.hpp>
#include <nonius/detail/stats.hpp>
#include <nonius/detail/measure.hpp>
#include <nonius/detail/run_for_at_least.hpp>
#include <nonius/clock.hpp>

#include <algorithm>
#include <iterator>
#include <tuple>
#include <vector>
#include <cmath>

namespace nonius {
    namespace detail {
        template <typename Clock>
        std::vector<double> resolution(int k) {
            std::vector<TimePoint<Clock>> times;
            times.reserve(k+1);
            std::generate_n(std::back_inserter(times), k+1, now<Clock>{});

            std::vector<double> deltas;
            deltas.reserve(k);
            std::transform(std::next(times.begin()), times.end(), times.begin(),
                              std::back_inserter(deltas),
                              [](TimePoint<Clock> a, TimePoint<Clock> b) { return static_cast<double>((a - b).count()); });

            return deltas;
        }

        const auto warmup_seed = 10000;
        const auto clock_resolution_estimation_time = chrono::milliseconds(500);
        const auto clock_cost_estimation_time_limit = chrono::seconds(1);
        const auto clock_cost_estimation_tick_limit = 100000;
        const auto clock_cost_estimation_time = chrono::milliseconds(10);
        const auto clock_cost_estimation_iterations = 10000;

        template <typename Clock>
        int warmup() {
            return run_for_at_least<Clock>({}, chrono::duration_cast<Duration<Clock>>(warmup_time), warmup_seed, &resolution<Clock>)
                    .iterations;
        }
        template <typename Clock>
        environment_estimate<FloatDuration<Clock>> estimate_clock_resolution(int iterations) {
            auto r = run_for_at_least<Clock>({}, chrono::duration_cast<Duration<Clock>>(clock_resolution_estimation_time), iterations, &resolution<Clock>)
                    .result;
            return {
                FloatDuration<Clock>(mean(r.begin(), r.end())),
                classify_outliers(r.begin(), r.end()),
            };
        }
        template <typename Clock>
        environment_estimate<FloatDuration<Clock>> estimate_clock_cost(FloatDuration<Clock> resolution) {
            auto time_limit = std::min(resolution * clock_cost_estimation_tick_limit, FloatDuration<Clock>(clock_cost_estimation_time_limit));
            auto time_clock = [](int k) {
                return detail::measure<Clock>([k]{
                    for(int i = 0; i < k; ++i) {
                        volatile auto ignored = Clock::now();
                        (void)ignored;
                    }
                }).elapsed;
            };
            time_clock(1);
            int iters = clock_cost_estimation_iterations;
            auto&& r = run_for_at_least<Clock>({}, chrono::duration_cast<Duration<Clock>>(clock_cost_estimation_time), iters, time_clock);
            std::vector<double> times;
            int nsamples = static_cast<int>(std::ceil(time_limit / r.elapsed));
            times.reserve(nsamples);
            std::generate_n(std::back_inserter(times), nsamples, [time_clock, &r]{
                        return static_cast<double>((time_clock(r.iterations) / r.iterations).count());
                    });
            return {
                FloatDuration<Clock>(mean(times.begin(), times.end())),
                classify_outliers(times.begin(), times.end()),
            };
        }
    } // namespace detail
} // namespace nonius

#endif // NONIUS_DETAIL_ENVIRONMENT_HPP
