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

// Reporter interface

#ifndef NONIUS_REPORTER_HPP
#define NONIUS_REPORTER_HPP

#include <nonius/clock.hpp>
#include <nonius/outlier_classification.hpp>
#include <nonius/environment.hpp>
#include <nonius/execution_plan.hpp>
#include <nonius/sample_analysis.hpp>
#include <nonius/detail/noexcept.hpp>
#include <nonius/detail/unique_name.hpp>

#include <vector>
#include <string>
#include <ios>
#include <ostream>
#include <fstream>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <exception>

namespace nonius {
    struct bad_stream : virtual std::exception {
        char const* what() const NONIUS_NOEXCEPT override {
            return "failed to open file";
        }
    };

    struct reporter {
    public:
        virtual ~reporter() = default;

        void configure(configuration& cfg) {
            if(cfg.output_file.empty()) {
                os = [&]() -> std::ostream& { return std::cout; };
            } else {
                auto ofs = std::make_shared<std::ofstream>(cfg.output_file);
                os = [ofs]() -> std::ostream& { return *ofs; };
            }
            report_stream().exceptions(std::ios::failbit);
            if(!report_stream()) throw bad_stream();
            do_configure(cfg);
        }

        void warmup_start() {
            do_warmup_start();
        }
        void warmup_end(int iterations) {
            do_warmup_end(iterations);
        }
        void estimate_clock_resolution_start() {
            do_estimate_clock_resolution_start();
        }
        void estimate_clock_resolution_complete(environment_estimate<fp_seconds> estimate) {
            do_estimate_clock_resolution_complete(estimate);
        }

        void estimate_clock_cost_start() {
            do_estimate_clock_cost_start();
        }
        void estimate_clock_cost_complete(environment_estimate<fp_seconds> estimate) {
            do_estimate_clock_cost_complete(estimate);
        }

        void suite_start() {
            do_suite_start();
        }
        void params_start(parameters const& params) {
            do_params_start(params);
        }
        void benchmark_start(std::string const& name) {
            do_benchmark_start(name);
        }

        void measurement_start(execution_plan<fp_seconds> plan) {
            do_measurement_start(plan);
        }
        void measurement_complete(std::vector<fp_seconds> const& samples) {
            do_measurement_complete(samples);
        }

        void analysis_start() {
            do_analysis_start();
        }
        void analysis_complete(sample_analysis<fp_seconds> const& analysis) {
            do_analysis_complete(analysis);
        }

        void benchmark_failure(std::exception_ptr error) {
            do_benchmark_failure(error);
        }
        void benchmark_complete() {
            do_benchmark_complete();
        }

        void params_complete() {
            do_params_complete();
        }
        void suite_complete() {
            do_suite_complete();
        }

        virtual std::string description() = 0;

    private:
        virtual void do_configure(configuration& /*cfg*/) {}

        virtual void do_warmup_start() {}
        virtual void do_warmup_end(int /*iterations*/) {}

        virtual void do_estimate_clock_resolution_start() {}
        virtual void do_estimate_clock_resolution_complete(environment_estimate<fp_seconds> /*estimate*/) {}

        virtual void do_estimate_clock_cost_start() {}
        virtual void do_estimate_clock_cost_complete(environment_estimate<fp_seconds> /*estimate*/) {}

        virtual void do_suite_start() {}
        virtual void do_params_start(parameters const& /*params*/) {}
        virtual void do_benchmark_start(std::string const& /*name*/) {}

        virtual void do_measurement_start(execution_plan<fp_seconds> /*plan*/) {}
        virtual void do_measurement_complete(std::vector<fp_seconds> const& /*samples*/) {}

        virtual void do_analysis_start() {} // TODO make generic?
        virtual void do_analysis_complete(sample_analysis<fp_seconds> const& /*analysis*/) {}

        virtual void do_benchmark_failure(std::exception_ptr /*error*/) {}
        virtual void do_benchmark_complete() {}
        virtual void do_params_complete() {}
        virtual void do_suite_complete() {}

    protected:
        std::ostream& progress_stream() {
            return std::cout;
        }
        std::ostream& error_stream() {
            return std::cerr;
        }
        std::ostream& report_stream() {
            return os();
        }

    private:
        std::function<std::ostream&()> os;
    };

    using reporter_registry = std::unordered_map<std::string, std::unique_ptr<reporter>>;

    inline reporter_registry& global_reporter_registry() {
        static reporter_registry registry;
        return registry;
    }

    struct reporter_registrar {
        reporter_registrar(reporter_registry& registry, std::string name, reporter* registrant) {
            registry.emplace(std::move(name), std::unique_ptr<reporter>(registrant));
        }
    };
} // namespace nonius

#define NONIUS_REPORTER(name, ...) \
    namespace { static ::nonius::reporter_registrar NONIUS_DETAIL_UNIQUE_NAME(reporter_registrar) (::nonius::global_reporter_registry(), name, new __VA_ARGS__()); } \
    static_assert(true, "")

#endif // NONIUS_REPORTER_HPP
