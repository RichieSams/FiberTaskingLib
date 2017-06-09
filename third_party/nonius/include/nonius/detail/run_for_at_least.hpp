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

// Run a function for a minimum amount of time

#ifndef NONIUS_RUN_FOR_AT_LEAST_HPP
#define NONIUS_RUN_FOR_AT_LEAST_HPP

#include <nonius/clock.hpp>
#include <nonius/chronometer.hpp>
#include <nonius/detail/measure.hpp>
#include <nonius/detail/complete_invoke.hpp>
#include <nonius/detail/timing.hpp>
#include <nonius/detail/meta.hpp>

#include <utility>
#include <type_traits>

namespace nonius {
    namespace detail {
        template <typename Clock, typename Fun>
        TimingOf<Clock, Fun(int)> measure_one(Fun&& fun, int iters, const parameters&, std::false_type) {
            return detail::measure<Clock>(fun, iters);
        }
        template <typename Clock, typename Fun>
        TimingOf<Clock, Fun(chronometer)> measure_one(Fun&& fun, int iters, const parameters& params, std::true_type) {
            detail::chronometer_model<Clock> meter;
            auto&& result = detail::complete_invoke(fun, chronometer(meter, iters, params));

            return { meter.elapsed(), std::move(result), iters };
        }

        template <typename Clock, typename Fun>
        using run_for_at_least_argument_t = typename std::conditional<detail::is_callable<Fun(chronometer)>::value, chronometer, int>::type;

        struct optimized_away_error : std::exception {
            const char* what() const NONIUS_NOEXCEPT override {
                return "could not measure benchmark, maybe it was optimized away";
            }
        };

        template <typename Clock, typename Fun>
        TimingOf<Clock, Fun(run_for_at_least_argument_t<Clock, Fun>)> run_for_at_least(const parameters& params, Duration<Clock> how_long, int seed, Fun&& fun) {
            auto iters = seed;
            while(iters < (1 << 30)) {
                auto&& timing = measure_one<Clock>(fun, iters, params, detail::is_callable<Fun(chronometer)>());

                if(timing.elapsed >= how_long) {
                    return { timing.elapsed, std::move(timing.result), iters };
                }
                iters *= 2;
            }
            throw optimized_away_error{};
        }
    } // namespace detail
} // namespace nonius

#endif // NONIUS_RUN_FOR_AT_LEAST_HPP
