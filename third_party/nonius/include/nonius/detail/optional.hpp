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

// Simple optional type implementation partially based on C++17 interface

#ifndef NONIUS_OPTIONAL_HPP
#define NONIUS_OPTIONAL_HPP

#include <type_traits>
#include <cassert>

namespace nonius {
    struct nullopt_t {
        constexpr nullopt_t(int) {}
    };
    constexpr nullopt_t nullopt() { return {0}; }

    template <typename T>
    class optional {
        typename std::aligned_storage<sizeof(T), alignof(T)>::type storage;
        bool initialized;

        T& get_impl() {
            assert(initialized);
            return *reinterpret_cast<T*>(&storage);
        }
        T const& get_impl() const {
            assert(initialized);
            return *reinterpret_cast<T const*>(&storage);
        }

        void construct(T const& value) {
            assert(!initialized);
            new (&storage) T(value);
            initialized = true;
        }
        void construct(T&& value) {
            assert(!initialized);
            new (&storage) T(std::move(value));
            initialized = true;
        }
        void destroy() {
            if (initialized) {
                get_impl().~T();
                initialized = false;
            }
        }
        optional<T>& assign(T const& value) {
            destroy();
            construct(value);
            return *this;
        }
        optional<T>& assign(T&& value) {
            destroy();
            construct(std::move(value));
            return *this;
        }
    public:
        constexpr optional() : initialized(false) {}
        constexpr optional(nullopt_t) : initialized(false) {}
        optional(optional<T> const& rhs) : initialized(false) {
            if (rhs.initialized) {
                construct(rhs.get_impl());
            }
        }
        optional(optional<T>&& rhs) : initialized(false) {
            if (rhs.initialized) {
                construct(std::move(rhs.get_impl()));
            }
        }
        optional(T const& value) : initialized(false) {
            construct(value);
        }
        optional(T&& value) : initialized(false) {
            construct(std::move(value));
        }
        ~optional() {
            destroy();
        }

        optional<T>& operator=(optional<T> const& rhs) {
            destroy();
            if (rhs.initialized) {
                construct(rhs.get_impl());
            }
            return *this;
        }
        optional<T>& operator=(optional<T> && rhs) {
            destroy();
            if (rhs.initialized) {
                construct(std::move(rhs.get_impl()));
            }
            return *this;
        }
        optional<T>& operator=(T const& rhs) {
            destroy();
            construct(rhs);
            return *this;
        }
        optional<T>& operator=(T&& rhs) {
            destroy();
            construct(std::move(rhs));
            return *this;
        }
        optional<T>& operator=(nullopt_t) {
            destroy();
            return *this;
        }

        constexpr T const* operator->() const { return &get_impl(); }
        T* operator->() { return &get_impl(); }

        constexpr T const& operator*() const { return get_impl(); }
        T& operator*() { return get_impl(); }

        constexpr bool has_value() const { return initialized; }
        constexpr explicit operator bool() const { return initialized; }

        constexpr bool operator!() const { return !initialized; }
    };
} // namespace nonius

#endif // NONIUS_OPTIONAL_HPP

