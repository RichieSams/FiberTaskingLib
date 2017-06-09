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

// Dumb std::function implementation for consistent call overhead

#ifndef NONIUS_DETAIL_BENCHMARK_FUNCTION_HPP
#define NONIUS_DETAIL_BENCHMARK_FUNCTION_HPP

#include <nonius/chronometer.hpp>
#include <nonius/detail/complete_invoke.hpp>
#include <nonius/detail/meta.hpp>

#include <cassert>
#include <type_traits>
#include <utility>
#include <memory>

namespace nonius {
    namespace detail {
        template <typename T>
        using Decay = typename std::decay<T>::type;
        template <typename T, typename U>
        struct is_related
        : std::is_same<Decay<T>, Decay<U>> {};

        /// We need to reinvent std::function because every piece of code that might add overhead
        /// in a measurement context needs to have consistent performance characteristics so that we
        /// can account for it in the measurement.
        /// Implementations of std::function with optimizations that aren't always applicable, like
        /// small buffer optimizations, are not uncommon.
        /// This is effectively an implementation of std::function without any such optimizations;
        /// it may be slow, but it is consistently slow.
        struct benchmark_function {
        private:
            struct concept {
                virtual benchmark_function call(parameters params) const = 0;
                virtual void call(chronometer meter) const = 0;
                virtual concept* clone() const = 0;
                virtual ~concept() = default;
            };
            template <typename Fun>
            struct model : public concept {
                model(Fun&& fun) : fun(std::move(fun)) {}
                model(Fun const& fun) : fun(fun) {}

                model<Fun>* clone() const override { return new model<Fun>(*this); }

                benchmark_function call(parameters params) const override {
                    return call(params, is_callable<Fun(parameters)>());
                }
                benchmark_function call(parameters params, std::true_type) const {
                    return fun(params);
                }
                benchmark_function call(parameters, std::false_type) const {
                    return this->clone();
                }

                void call(chronometer meter) const override {
                    call(meter, is_callable<Fun(chronometer)>(), is_callable<Fun(parameters)>());
                }
                void call(chronometer meter, std::true_type, std::false_type) const {
                    fun(meter);
                }
                void call(chronometer meter, std::false_type, std::false_type) const {
                    meter.measure(fun);
                }
                template <typename T>
                void call(chronometer, T, std::true_type) const {
                    // the function should be prepared first
                    assert(false);
                }

                Fun fun;
            };

            struct do_nothing { void operator()() const {} };

            template <typename T>
            benchmark_function(model<T>* c) : f(c) {}

        public:
            benchmark_function()
            : f(new model<do_nothing>{{}})
            {}

            template <typename Fun,
                      typename std::enable_if<!is_related<Fun, benchmark_function>::value, int>::type = 0>
            benchmark_function(Fun&& fun)
            : f(new model<typename std::decay<Fun>::type>(std::forward<Fun>(fun))) {}

            benchmark_function(benchmark_function&& that)
            : f(std::move(that.f)) {}

            benchmark_function(benchmark_function const& that)
            : f(that.f->clone()) {}

            benchmark_function& operator=(benchmark_function&& that) {
                f = std::move(that.f);
                return *this;
            }

            benchmark_function& operator=(benchmark_function const& that) {
                f.reset(that.f->clone());
                return *this;
            }

            benchmark_function operator()(parameters params) const { return f->call(params); }
            void operator()(chronometer meter) const { f->call(meter); }
        private:
            std::unique_ptr<concept> f;
        };
    } // namespace detail
} // namespace nonius

#endif // NONIUS_DETAIL_BENCHMARK_FUNCTION_HPP
