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

// Run and analyse one benchmark

#ifndef NONIUS_DETAIL_ANALYSE_HPP
#define NONIUS_DETAIL_ANALYSE_HPP

#include <nonius/clock.hpp>
#include <nonius/sample_analysis.hpp>
#include <nonius/detail/stats.hpp>

#include <algorithm>
#include <iterator>
#include <vector>

namespace nonius {
    namespace detail {
        template <typename Duration, typename Iterator>
        sample_analysis<Duration> analyse(configuration cfg, environment<Duration>, Iterator first, Iterator last) {
            std::vector<double> samples;
            samples.reserve(last - first);
            std::transform(first, last, std::back_inserter(samples), [](Duration d) { return d.count(); });

            auto analysis = nonius::detail::analyse_samples(cfg.confidence_interval, cfg.resamples, samples.begin(), samples.end());
            auto outliers = nonius::detail::classify_outliers(samples.begin(), samples.end());

            auto wrap_estimate = [](estimate<double> e) {
                return estimate<Duration> {
                    Duration(e.point),
                    Duration(e.lower_bound),
                    Duration(e.upper_bound),
                    e.confidence_interval,
                };
            };
            std::vector<Duration> samples2;
            samples2.reserve(samples.size());
            std::transform(samples.begin(), samples.end(), std::back_inserter(samples2), [](double d) { return Duration(d); });
            return {
                std::move(samples2),
                wrap_estimate(analysis.mean),
                wrap_estimate(analysis.standard_deviation),
                outliers,
                analysis.outlier_variance,
            };
        }
    } // namespace detail
} // namespace nonius

#endif // NONIUS_DETAIL_ANALYSE_HPP

