// Nonius - C++ benchmarking tool
//
// Written in 2016 by Martinho Fernandes <rmf@rmf.io>
//
// To the extent possible under law, the author(s) have dedicated all copyright and related
// and neighboring rights to this software to the public domain worldwide. This software is
// distributed without any warranty.
//
// You should have received a copy of the CC0 Public Domain Dedication along with this software.
// If not, see <http://creativecommons.org/publicdomain/zero/1.0/>

// Hinting the optimizer

#ifndef NONIUS_OPTIMIZER_HPP
#define NONIUS_OPTIMIZER_HPP

#if defined(NONIUS_MSVC)
#   include <atomic> // atomic_thread_fence
#endif

namespace nonius {
#if defined(NONIUS_GCC) || defined(NONIUS_CLANG)
    template <typename T>
    inline void keep_memory(T* p) {
        asm volatile("" : : "g"(p) : "memory");
    }
    inline void keep_memory() {
        asm volatile("" : : : "memory");
    }

    namespace detail {
        inline void optimizer_barrier() { keep_memory(); }
    } // namespace detail
#elif defined(NONIUS_MSVC)

#pragma optimize("", off)
    template <typename T>
    inline void keep_memory(T* p) {
        // thanks @milleniumbug
        *reinterpret_cast<char volatile*>(p) = *reinterpret_cast<char const volatile*>(p);
    }
    // TODO equivalent keep_memory()
#pragma optimize("", on)

    namespace detail {
        inline void optimizer_barrier() {
            std::atomic_thread_fence(std::memory_order_seq_cst);
        }
    } // namespace detail

#endif
} // namespace nonius

#endif // NONIUS_OPTIMIZER_HPP

