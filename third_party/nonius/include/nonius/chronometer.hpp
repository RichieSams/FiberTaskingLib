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

// User-facing chronometer

#ifndef NONIUS_CHRONOMETER_HPP
#define NONIUS_CHRONOMETER_HPP

#include <nonius/clock.hpp>
#include <nonius/detail/complete_invoke.hpp>
#include <nonius/detail/meta.hpp>
#include <nonius/param.hpp>

namespace nonius {
    namespace detail {
        struct chronometer_concept {
            virtual void start() = 0;
            virtual void finish() = 0;
            virtual ~chronometer_concept() = default;
        };
        template <typename Clock>
        struct chronometer_model final : public chronometer_concept {
            void start() override { started = Clock::now(); }
            void finish() override { finished = Clock::now(); }

            Duration<Clock> elapsed() const { return finished - started; }

            TimePoint<Clock> started;
            TimePoint<Clock> finished;
        };
    } // namespace detail

    struct chronometer {
    public:
        template <typename Fun>
        void measure(Fun&& fun) { measure(std::forward<Fun>(fun), detail::is_callable<Fun(int)>()); }

        int runs() const { return k; }

        chronometer(detail::chronometer_concept& meter, int k, const parameters& p)
            : impl(&meter)
            , k(k)
            , params(&p)
        {}

        template <typename Tag>
        auto param() const -> typename Tag::type {
            return params->get<Tag>();
        }

    private:
        template <typename Fun>
        void measure(Fun&& fun, std::false_type) {
            measure([&fun](int) { fun(); });
        }
        template <typename Fun>
        void measure(Fun&& fun, std::true_type) {
            impl->start();
            for(int i = 0; i < k; ++i) fun(i);
            impl->finish();
        }

        detail::chronometer_concept* impl;
        int k;
        const parameters* params;
    };
} // namespace nonius

#endif // NONIUS_CHRONOMETER_HPP
