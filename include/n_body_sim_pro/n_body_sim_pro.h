#ifndef N_BODY_SIM_PRO_H
#define N_BODY_SIM_PRO_H

/*
 * Umbrella header for the public C API.
 *
 * The C engine is a C17 numerical/HPC library. It exposes a plain C ABI so
 * it can be wrapped by the C++ application layer or consumed directly from C.
 */

#include "n_body_sim_pro/barnes_hut/barnes_hut.h"
#include "n_body_sim_pro/checkpoint/checkpoint.h"
#include "n_body_sim_pro/core/particle_system.h"
#include "n_body_sim_pro/core/status.h"
#include "n_body_sim_pro/core/vector.h"
#include "n_body_sim_pro/diagnostics/numerics.h"
#include "n_body_sim_pro/generation/presets.h"
#include "n_body_sim_pro/generation/random.h"
#include "n_body_sim_pro/memory/allocator.h"
#include "n_body_sim_pro/memory/allocation_tracker.h"
#include "n_body_sim_pro/memory/memory_estimate.h"
#include "n_body_sim_pro/mpi/distributed_barnes_hut.h"
#include "n_body_sim_pro/mpi/mpi_runtime.h"
#include "n_body_sim_pro/physics/gravity.h"
#include "n_body_sim_pro/physics/integrator.h"
#include "n_body_sim_pro/simd/backend.h"
#include "n_body_sim_pro/simd/cpu.h"
#include "n_body_sim_pro/threading/threading.h"
#include "n_body_sim_pro/threading/numa.h"

#endif /* N_BODY_SIM_PRO_H */
