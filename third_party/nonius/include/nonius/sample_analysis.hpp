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

// Benchmark results

#ifndef NONIUS_BENCHMARK_RESULTS_HPP
#define NONIUS_BENCHMARK_RESULTS_HPP

#include <nonius/clock.hpp>
#include <nonius/estimate.hpp>
#include <nonius/outlier_classification.hpp>

#include <algorithm>
#include <vector>
#include <string>
#include <iterator>

namespace nonius {
    template <typename Duration>
    struct sample_analysis {
        std::vector<Duration> samples;
        estimate<Duration> mean;
        estimate<Duration> standard_deviation;
        outlier_classification outliers;
        double outlier_variance;

        template <typename Duration2>
        operator sample_analysis<Duration2>() const {
            std::vector<Duration2> samples2;
            samples2.reserve(samples.size());
            std::transform(samples.begin(), samples.end(), std::back_inserter(samples2), [](Duration d) { return Duration2(d); });
            return {
                std::move(samples2),
                mean,
                standard_deviation,
                outliers,
                outlier_variance,
            };
        }
    };
} // namespace nonius

#endif // NONIUS_BENCHMARK_RESULTS_HPP

