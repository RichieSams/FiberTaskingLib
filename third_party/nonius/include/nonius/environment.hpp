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

// Environment information

#ifndef NONIUS_ENVIRONMENT_HPP
#define NONIUS_ENVIRONMENT_HPP

#include <nonius/clock.hpp>
#include <nonius/outlier_classification.hpp>

namespace nonius {
    template <typename Duration>
    struct environment_estimate {
        Duration mean = {};
        outlier_classification outliers;

        environment_estimate() = default;

        environment_estimate(Duration mean, outlier_classification outliers)
            : mean(mean)
            , outliers(outliers)
        {}

        template <typename Duration2>
        operator environment_estimate<Duration2>() const {
            return { mean, outliers };
        }
    };
    template <typename Clock>
    struct environment {
        using clock_type = Clock;
        environment_estimate<FloatDuration<Clock>> clock_resolution;
        environment_estimate<FloatDuration<Clock>> clock_cost;
        //estimate function_cost;
    };
} // namespace nonius

#endif // NONIUS_ENVIRONMENT_HPP
