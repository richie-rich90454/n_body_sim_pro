#ifndef HPCSIM_H
#define HPCSIM_H

/*
 * Umbrella header for the public C API.
 *
 * The C engine is a C17 numerical/HPC library. It exposes a plain C ABI so
 * it can be wrapped by the C++ application layer or consumed directly from C.
 */

#include "hpcsim/barnes_hut/barnes_hut.h"
#include "hpcsim/checkpoint/checkpoint.h"
#include "hpcsim/core/particle_system.h"
#include "hpcsim/core/status.h"
#include "hpcsim/core/vector.h"
#include "hpcsim/diagnostics/numerics.h"
#include "hpcsim/generation/presets.h"
#include "hpcsim/generation/random.h"
#include "hpcsim/memory/allocator.h"
#include "hpcsim/memory/allocation_tracker.h"
#include "hpcsim/memory/memory_estimate.h"
#include "hpcsim/physics/gravity.h"
#include "hpcsim/physics/integrator.h"
#include "hpcsim/simd/backend.h"
#include "hpcsim/simd/cpu.h"
#include "hpcsim/threading/threading.h"
#include "hpcsim/threading/numa.h"

#endif /* HPCSIM_H */
