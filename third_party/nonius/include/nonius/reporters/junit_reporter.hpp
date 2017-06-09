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

// JUnit reporter

#ifndef NONIUS_REPORTERS_JUNIT_REPORTER_HPP
#define NONIUS_REPORTERS_JUNIT_REPORTER_HPP

#include <nonius/reporter.hpp>
#include <nonius/configuration.hpp>
#include <nonius/sample_analysis.hpp>
#include <nonius/execution_plan.hpp>
#include <nonius/environment.hpp>
#include <nonius/detail/pretty_print.hpp>
#include <nonius/detail/escape.hpp>

#include <ios>
#include <iomanip>
#include <algorithm>
#include <string>
#include <iterator>
#include <limits>
#include <unordered_map>
#include <vector>
#include <exception>

namespace nonius {
    struct junit_reporter : reporter {
    private:
        std::string description() override {
            return "outputs results to a JUnit-compatible XML file";
        }

        void do_configure(configuration& cfg) override {
            n_samples = cfg.samples;
            confidence_interval = cfg.confidence_interval;
            resamples = cfg.resamples;
            verbose = cfg.verbose;
            title = cfg.title;
        }

        struct result {
            sample_analysis<fp_seconds> analysis;
            std::exception_ptr failure;
        };

        void do_warmup_start() override {
            if(verbose) progress_stream() << "warming up\n";
        }
        void do_estimate_clock_resolution_start() override {
            if(verbose) progress_stream() << "estimating clock resolution\n";
        }
        void do_estimate_clock_cost_start() override {
            if(verbose) progress_stream() << "estimating cost of a clock call\n";
        }

        void do_benchmark_start(std::string const& name) override {
            if(verbose) progress_stream() << "\nbenchmarking " << name << "\n";
            current = name;
        }

        void do_measurement_start(execution_plan<fp_seconds> plan) override {
            report_stream() << std::setprecision(7);
            report_stream().unsetf(std::ios::floatfield);
            if(verbose) progress_stream() << "collecting " << n_samples << " samples, " << plan.iterations_per_sample << " iterations each, in estimated " << detail::pretty_duration(plan.estimated_duration) << "\n";
        }

        void do_analysis_start() override {
            if(verbose) report_stream() << "analysing samples\n";
        }
        void do_analysis_complete(sample_analysis<fp_seconds> const& analysis) override {
            data[current] = { analysis, nullptr };
        }

        void do_benchmark_failure(std::exception_ptr e) override {
            data[current] = { sample_analysis<fp_seconds>(), e };
            error_stream() << current << " failed to run successfully\n";
        }

        void do_suite_complete() override {
            if(verbose) progress_stream() << "\ngenerating JUnit report\n";

            report_stream() << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
            report_stream() << "<testsuite name=\"" << escape(title) << "\" tests=\"" << data.size() << "\"";
            auto failures = std::count_if(data.begin(), data.end(),
                    [](std::pair<std::string const&, result> const& p) {
                        return static_cast<bool>(p.second.failure);
                    });
            if(failures > 0) report_stream() << " errors=\"" << failures << "\"";
            report_stream() << ">\n";

            report_stream() << " <properties>\n";
            report_stream() << "  <property name=\"samples\" value=\"" << n_samples << "\">\n";
            report_stream() << "  <property name=\"confidence_interval\" value=\"" << std::setprecision(3) << confidence_interval << "\">\n";
            report_stream() << "  <property name=\"resamples\" value=\"" << resamples << "\">\n";
            report_stream() << " </properties>\n";

            for(auto tc : data) {
                report_stream() << " <testcase name=\"" << escape(tc.first) << "\"";
                if(tc.second.failure) {
                    report_stream() << ">\n";
                    try {
                        std::rethrow_exception(tc.second.failure);
                    } catch(std::exception const& e) {
                        report_stream() << "  <error message=\"" << escape(e.what()) << "\" />\n";
                    } catch(...) {
                        report_stream() << "  <error message=\"unknown error\" />\n";
                    }
                    report_stream() << " </testcase>\n";
                } else {
                    report_stream() << std::fixed;
                    report_stream().precision(std::numeric_limits<double>::digits10);
                    report_stream() << " time=\"" << tc.second.analysis.mean.point.count() << "\" />\n";
                }
            }

            report_stream() << "</testsuite>\n";

            report_stream() << std::flush;

            if(verbose) progress_stream() << "done\n";
        }

        static std::string escape(std::string const& source) {
            static const std::unordered_map<char, std::string> escapes {
                { '\'', "&apos;" },
                { '"',  "&quot;" },
                { '<',  "&lt;"   },
                { '>',  "&gt;"   },
                { '&',  "&amp;"  },
            };
            return detail::escape(source, escapes);
        }

        int n_samples;
        double confidence_interval;
        int resamples;
        bool verbose;
        std::string title;

        std::string current;
        std::unordered_map<std::string, result> data;
    };

    NONIUS_REPORTER("junit", junit_reporter);
} // namespace nonius

#endif // NONIUS_REPORTERS_JUNIT_REPORTER_HPP

