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

// Timing

#ifndef NONIUS_DETAIL_TIMING_HPP
#define NONIUS_DETAIL_TIMING_HPP

#include <nonius/clock.hpp>
#include <nonius/detail/complete_invoke.hpp>

#include <tuple>
#include <type_traits>

namespace nonius {
    template <typename Duration, typename Result>
    struct timing {
        Duration elapsed = {};
        Result result;
        int iterations = 0;

        timing() = default;

        timing(Duration elapsed, Result result, int iterations)
            : elapsed(elapsed)
            , result(result)
            , iterations(iterations)
        {}

        template <typename Duration2>
        operator timing<Duration2, Result>() const {
            return timing<Duration2, Result>{ chrono::duration_cast<Duration2>(elapsed), result, iterations };
        }
    };
    template <typename Clock, typename Sig>
    using TimingOf = timing<Duration<Clock>, detail::CompleteType<detail::ResultOf<Sig>>>;
} // namespace nonius

#endif // NONIUS_DETAIL_TIMING_HPP

