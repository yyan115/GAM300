// Implementation of the live registry and safe iteration semantics.
// - Includes: data structures mapping instance handles to registry refs, thread-safety notes, and utilities for stopping/resuming scripts.
// - Contains: iterator utilities that tolerate removal during iteration (common when scripts destroy themselves).
// - Use cases: central store used by engine loop to call all script Update functions, and by the editor.
#include "pch.h"