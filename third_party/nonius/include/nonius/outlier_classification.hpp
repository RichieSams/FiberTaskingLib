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

// Outlier information

#ifndef NONIUS_OUTLIERS_HPP
#define NONIUS_OUTLIERS_HPP

namespace nonius {
    struct outlier_classification {
        int samples_seen = 0;
        int low_severe = 0;     // more than 3 times IQR below Q1
        int low_mild = 0;       // 1.5 to 3 times IQR below Q1
        int high_mild = 0;      // 1.5 to 3 times IQR above Q3
        int high_severe = 0;    // more than 3 times IQR above Q3

        int total() const {
            return low_severe + low_mild + high_mild + high_severe;
        }
    };
} // namespace nonius

#endif // NONIUS_OUTLIERS_HPP

