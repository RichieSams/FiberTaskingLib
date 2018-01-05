//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

// Modified for use in FiberTaskingLib
//
// FiberTaskingLib is the legal property of Adrian Astley
// Copyright Adrian Astley 2015 - 2018



#pragma once

#include <cstdint>
// intptr_t

#include <cstddef>
// size_t

namespace boost_context {

typedef void *fcontext_t;

extern "C" void jump_fcontext(fcontext_t *from, fcontext_t to, void *arg);
extern "C" fcontext_t make_fcontext(void * sp, std::size_t size, void(*func)(void *));
// sp is the pointer to the _top_ of the stack (ie &stack_buffer[size]).

}
