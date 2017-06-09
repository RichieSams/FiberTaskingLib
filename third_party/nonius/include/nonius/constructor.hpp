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

// Constructor and destructor helpers

#ifndef NONIUS_CONSTRUCTOR_HPP
#define NONIUS_CONSTRUCTOR_HPP

#include <type_traits>

namespace nonius {
    namespace detail {
        template <typename T, bool Destruct>
        struct object_storage
        {
            typedef typename std::aligned_storage<sizeof(T), std::alignment_of<T>::value>::type TStorage;

            object_storage() : data() {}

            object_storage(const object_storage& other)
            {
                new(&data) T(other.stored_object());
            }

            object_storage(object_storage&& other)
            {
                new(&data) T(std::move(other.stored_object()));
            }

            ~object_storage() { destruct_on_exit<T>(); }

            template <typename... Args>
            void construct(Args&&... args)
            {
                new (&data) T(std::forward<Args>(args)...);
            }

            template <bool AllowManualDestruction = !Destruct>
            typename std::enable_if<AllowManualDestruction>::type destruct()
            {
                stored_object().~T();
            }

        private:
            // If this is a constructor benchmark, destruct the underlying object
            template <typename U>
            void destruct_on_exit(typename std::enable_if<Destruct, U>::type* = 0) { destruct<true>(); }
            // Otherwise, don't
            template <typename U>
            void destruct_on_exit(typename std::enable_if<!Destruct, U>::type* = 0) { }

            T& stored_object()
            {
                return *static_cast<T*>(static_cast<void*>(&data));
            }

            TStorage data;
        };
    }

    template <typename T>
    using storage_for = detail::object_storage<T, true>;

    template <typename T>
    using destructable_object = detail::object_storage<T, false>;
}

#endif // NONIUS_CONSTRUCTOR_HPP
