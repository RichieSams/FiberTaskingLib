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

// CSV raw data reporter

#ifndef NONIUS_REPORTERS_CSV_REPORTER_HPP
#define NONIUS_REPORTERS_CSV_REPORTER_HPP

#include <nonius/reporter.hpp>
#include <nonius/configuration.hpp>
#include <nonius/sample_analysis.hpp>
#include <nonius/execution_plan.hpp>
#include <nonius/environment.hpp>
#include <nonius/detail/pretty_print.hpp>

#include <ios>
#include <iomanip>
#include <algorithm>
#include <string>
#include <iterator>
#include <limits>
#include <unordered_map>
#include <vector>

namespace nonius {
    struct csv_reporter : reporter {
    private:
        std::string description() override {
            return "outputs samples to a CSV file";
        }

        void do_configure(configuration& cfg) override {
            cfg.no_analysis = true;
            n_samples = cfg.samples;
            verbose = cfg.verbose;
        }

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
        void do_measurement_complete(std::vector<fp_seconds> const& samples) override {
            data[current] = samples;
        }

        void do_benchmark_failure(std::exception_ptr) override {
            error_stream() << current << " failed to run successfully\n";
        }

        void do_suite_complete() override {
            if(verbose) progress_stream() << "\ngenerating CSV report\n";
            report_stream() << std::fixed;
            report_stream().precision(std::numeric_limits<double>::digits10);
            bool first = true;
            for(auto&& kv : data) {
                if(!first) report_stream() << ",";
                report_stream() << "\"" << escape(kv.first) << "\""; // TODO escape
                first = false;
            }
            report_stream() << "\n";
            for(int i = 0; i < n_samples; ++i) {
                first = true;
                for(auto&& kv : data) {
                    if(!first) report_stream() << ",";
                    report_stream() << kv.second[i].count();
                    first = false;
                }
                report_stream() << "\n";
            }
            if(verbose) progress_stream() << "done\n";
        }

    private:
        static std::string escape(std::string const& source) {
            auto first = source.begin();
            auto last = source.end();

            auto quotes = std::count(first, last, '"');
            if(quotes == 0) return source;

            std::string escaped;
            escaped.reserve(source.size() + quotes);

            while(first != last) {
                auto next_quote = std::find(first, last, '"');
                std::copy(first, next_quote, std::back_inserter(escaped));
                first = next_quote;
                if(first != last) {
                    ++first;
                    escaped.push_back('"');
                    escaped.push_back('"');
                }
            }

            return escaped;
        }

        int n_samples;
        bool verbose;
        std::string current;
        std::unordered_map<std::string, std::vector<fp_seconds>> data;
    };

    NONIUS_REPORTER("csv", csv_reporter);
} // namespace nonius

#endif // NONIUS_REPORTERS_CSV_REPORTER_HPP

