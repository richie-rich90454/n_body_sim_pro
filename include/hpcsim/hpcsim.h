#ifndef HPCSIM_H
#define HPCSIM_H

/*
 * Umbrella header for the public C API.
 *
 * The C engine is a C17 numerical/HPC library. It exposes a plain C ABI so
 * it can be wrapped by the C++ application layer or consumed directly from C.
 */

#include "hpcsim/core/particle_system.h"
#include "hpcsim/core/status.h"
#include "hpcsim/core/vector.h"
#include "hpcsim/diagnostics/numerics.h"
#include "hpcsim/memory/allocator.h"
#include "hpcsim/physics/gravity.h"
#include "hpcsim/physics/integrator.h"

#endif /* HPCSIM_H */
