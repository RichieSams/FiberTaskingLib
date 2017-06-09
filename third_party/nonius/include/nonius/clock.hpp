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

// Clocks

#ifndef NONIUS_CLOCK_HPP
#define NONIUS_CLOCK_HPP

#include <nonius/detail/compiler.hpp>

// MSVC <chrono> is borken and had little to no testing done before shipping (Dev14/VS15 CTP fixes it)
#if defined(NONIUS_MSVC) && NONIUS_MSVC < 1900
#   ifndef NONIUS_USE_BOOST_CHRONO
#       define NONIUS_USE_BOOST_CHRONO
#   endif
#endif

#ifdef NONIUS_USE_BOOST_CHRONO
#   include <boost/chrono.hpp>
#else
#   include <chrono>
#   include <ratio>
#endif

namespace nonius {
#ifdef NONIUS_USE_BOOST_CHRONO
    namespace chrono = boost::chrono;
    template <unsigned Num, unsigned Den = 1>
    using ratio = boost::ratio<Num, Den>;
#else
    namespace chrono = std::chrono;
    template <unsigned Num, unsigned Den = 1>
    using ratio = std::ratio<Num, Den>;
#endif
    using milli = ratio<1,       1000>;
    using micro = ratio<1,    1000000>;
    using nano  = ratio<1, 1000000000>;

    template <typename Clock>
    using Duration = typename Clock::duration;
    template <typename Clock>
    using FloatDuration = chrono::duration<double, typename Clock::period>;

    template <typename Clock>
    using TimePoint = typename Clock::time_point;

    using default_clock = chrono::high_resolution_clock;

    template <typename Clock>
    struct now {
        TimePoint<Clock> operator()() const {
            return Clock::now();
        }
    };

    using fp_seconds = chrono::duration<double, ratio<1>>;
} // namespace nonius

#endif // NONIUS_CLOCK_HPP

