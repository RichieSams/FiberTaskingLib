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

// Standard reporter

#ifndef NONIUS_REPORTERS_STANDARD_REPORTER_HPP
#define NONIUS_REPORTERS_STANDARD_REPORTER_HPP

#include <nonius/reporter.hpp>
#include <nonius/configuration.hpp>
#include <nonius/sample_analysis.hpp>
#include <nonius/execution_plan.hpp>
#include <nonius/environment.hpp>
#include <nonius/clock.hpp>
#include <nonius/detail/pretty_print.hpp>

#include <ratio>
#include <ios>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <memory>

namespace nonius {
    struct standard_reporter : reporter {
    private:
        std::string description() override {
            return "the standard reporter";
        }

        void do_configure(configuration& cfg) override {
            n_samples = cfg.samples;
            verbose = cfg.verbose;
            summary = cfg.summary;
            n_resamples = cfg.resamples;
            if(!summary && !cfg.params.map.empty()) report_stream() << "parameters\n" << cfg.params.map;
        }

        void do_warmup_start() override {
            if(verbose) report_stream() << "warming up\n";
        }
        void do_estimate_clock_resolution_start() override {
            if(verbose) report_stream() << "estimating clock resolution\n";
        }
        void do_estimate_clock_resolution_complete(environment_estimate<fp_seconds> estimate) override {
            if(!summary) {
                if(!verbose) report_stream() << "clock resolution: ";
                print_environment_estimate(estimate, estimate.outliers.samples_seen + 2);
            }
        }

        void do_estimate_clock_cost_start() override {
            if(verbose) report_stream() << "estimating cost of a clock call\n";
        }
        void do_estimate_clock_cost_complete(environment_estimate<fp_seconds> estimate) override {
            if(verbose) print_environment_estimate(estimate, estimate.outliers.samples_seen);
        }

        void do_params_start(parameters const& params) override {
            if(!summary && !params.empty()) report_stream() << "\n\nnew round for parameters\n" << params;
        }
        void do_benchmark_start(std::string const& name) override {
            report_stream() << '\n';
            if(!summary) report_stream() << "benchmarking ";
            report_stream() << name << "\n";
            current = name;
        }

        void do_measurement_start(execution_plan<fp_seconds> plan) override {
            report_stream() << std::setprecision(7);
            report_stream().unsetf(std::ios::floatfield);
            if(!summary) report_stream() << "collecting " << n_samples << " samples, " << plan.iterations_per_sample << " iterations each, in estimated " << detail::pretty_duration(plan.estimated_duration) << "\n";
        }
        void do_analysis_start() override {
            if(verbose) report_stream() << "bootstrapping with " << n_resamples << " resamples\n";
        }
        void do_benchmark_failure(std::exception_ptr eptr) override {
            error_stream() << current << " failed to run successfully\n";
            if(!summary) {
                try {
                    std::rethrow_exception(eptr);
                } catch(std::exception& ex) {
                    error_stream() << "error: " << ex.what();
                } catch(...) {
                    error_stream() << "unknown error";
                }
            }
            report_stream() << "\nbenchmark aborted\n";
        }
        void do_analysis_complete(sample_analysis<fp_seconds> const& analysis) override {
            print_statistic_estimate("mean", analysis.mean);
            print_statistic_estimate("std dev", analysis.standard_deviation);
            if(!summary) print_outliers(analysis.outliers);
            if(verbose) report_stream() << "variance introduced by outliers: " << detail::percentage(analysis.outlier_variance) << "\n";
            const char* effect;
            if(analysis.outlier_variance < 0.01) {
                effect = "unaffected";
            } else if(analysis.outlier_variance < 0.1) {
                effect = "slightly inflated";
            } else if(analysis.outlier_variance < 0.5) {
                effect = "moderately inflated";
            } else {
                effect = "severely inflated";
            }
            report_stream() << "variance is " << effect << " by outliers\n";
        }

        void print_environment_estimate(environment_estimate<fp_seconds> e, int iterations) {
            report_stream() << std::setprecision(7);
            report_stream().unsetf(std::ios::floatfield);
            report_stream() << "mean is " << detail::pretty_duration(e.mean) << " (" << iterations << " iterations)\n";
            if(verbose) print_outliers(e.outliers);
        }
        void print_outlier_count(const char* description, int count, int total) {
            if(count > 0) report_stream() << "  " << count << " (" << detail::percentage_ratio(count, total) << ") " << description << "\n";
        }
        void print_outliers(outlier_classification o) {
            report_stream() << "found " << o.total() << " outliers among " << o.samples_seen << " samples (" << detail::percentage_ratio(o.total(), o.samples_seen) << ")\n";
            if(verbose) {
                print_outlier_count("low severe", o.low_severe, o.samples_seen);
                print_outlier_count("low mild", o.low_mild, o.samples_seen);
                print_outlier_count("high mild", o.high_mild, o.samples_seen);
                print_outlier_count("high severe", o.high_severe, o.samples_seen);
            }
        }
        void print_statistic_estimate(const char* name, estimate<fp_seconds> estimate) {
            report_stream() << std::setprecision(7);
            report_stream().unsetf(std::ios::floatfield);
            report_stream() << name << ": " << detail::pretty_duration(estimate.point);
            if(!summary) {
                report_stream() << ", lb " << detail::pretty_duration(estimate.lower_bound)
                         << ", ub " << detail::pretty_duration(estimate.upper_bound)
                         << ", ci " << std::setprecision(3) << estimate.confidence_interval;
            }
            report_stream() << "\n";
        }

        int n_samples = 0;
        int n_resamples = 0;
        bool verbose = false;
        bool summary = false;

        std::string current;
    };

    NONIUS_REPORTER("", standard_reporter);
    NONIUS_REPORTER("standard", standard_reporter);
} // namespace nonius

#endif // NONIUS_REPORTERS_STANDARD_REPORTER_HPP
