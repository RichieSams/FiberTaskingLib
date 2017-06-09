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

// Main header

#ifndef NONIUS_HPP
#define NONIUS_HPP

#include <nonius/clock.hpp>
#include <nonius/benchmark.hpp>
#include <nonius/constructor.hpp>
#include <nonius/configuration.hpp>
#include <nonius/chronometer.hpp>
#include <nonius/optimizer.hpp>
#include <nonius/go.hpp>
#include <nonius/param.hpp>

#include <nonius/reporters/standard_reporter.hpp>

#ifndef NONIUS_DISABLE_EXTRA_REPORTERS
#ifndef NONIUS_DISABLE_CSV_REPORTER
#include <nonius/reporters/csv_reporter.hpp>
#endif // NONIUS_DISABLE_CSV_REPORTER
#ifndef NONIUS_DISABLE_JUNIT_REPORTER
#include <nonius/reporters/junit_reporter.hpp>
#endif // NONIUS_DISABLE_JUNIT_REPORTER
#ifndef NONIUS_DISABLE_HTML_REPORTER
#include <nonius/reporters/html_reporter.hpp>
#endif // NONIUS_DISABLE_HTML_REPORTER
#endif // NONIUS_DISABLE_EXTRA_REPORTERS

#endif // NONIUS_HPP
