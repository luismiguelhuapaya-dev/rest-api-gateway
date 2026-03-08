#include "gateway/core/Coroutine.h"

// The Task<T> and Task<void> coroutine types are fully implemented
// as templates in the header file. This translation unit exists
// to ensure the header compiles correctly as part of the build
// and to provide a place for any future non-template coroutine
// utility implementations.

namespace Gateway
{
    // Reserved for future non-template coroutine utilities.
    // All coroutine machinery is defined in Coroutine.h as
    // template specializations for Task<T> and Task<void>.
} // namespace Gateway
