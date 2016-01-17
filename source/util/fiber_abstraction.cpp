/* FiberTaskingLib - A tasking library that uses fibers for efficient task switching
 *
 * This library was created as a proof of concept of the ideas presented by
 * Christian Gyrling in his 2015 GDC Talk 'Parallelizing the Naughty Dog Engine Using Fibers'
 *
 * http://gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine
 *
 * FiberTaskingLib is the legal property of Adrian Astley
 * Copyright Adrian Astley 2015
 */

#include "fiber_tasking_lib/config.h"

#if defined(BOOST_CONTEXT_FIBER_IMPL)

#include "fiber_tasking_lib/fiber_abstraction.h"
#include "fiber_tasking_lib/tls_abstraction.h"


namespace FiberTaskingLib {

// Boost.Context doesn't have a global way to get the current fiber
TLS_VARIABLE(FiberType, tls_currentFiber);

} // End of namespace FiberTaskingLib

#endif
