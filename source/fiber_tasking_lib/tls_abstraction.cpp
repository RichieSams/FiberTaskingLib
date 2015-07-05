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
#include "fiber_tasking_lib/typedefs.h"
#include "fiber_tasking_lib/thread_abstraction.h"


#if defined(FIBER_IMPL_SUPPORTS_TLS)
    namespace FiberTaskingLib {
        THREAD_LOCAL uint tls_threadId;
    } // End of namespace FiberTaskingLib
#else
    #include <unordered_map>

    namespace FiberTaskingLib {
        std::unordered_map<ThreadId, uint> g_threadIdToIndexMap;
    } // End of namespace FiberTaskingLib
#endif


