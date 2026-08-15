#ifndef N_BODY_SIM_PRO_SIMD_BACKEND_H
#define N_BODY_SIM_PRO_SIMD_BACKEND_H

#include "n_body_sim_pro/simd/cpu.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * SIMD backend selection and kernel dispatch.
 *
 * The "selected backend" is the backend whose kernel is actually used, not
 * merely the best ISA the CPU advertises. A backend is only selected when a
 * real implementation exists; see docs/performance/simd.md for the current
 * kernel coverage matrix.
 */

typedef enum NBodySimProSimdBackend {
    N_BODY_SIM_PRO_SIMD_BACKEND_SCALAR = 0,
    N_BODY_SIM_PRO_SIMD_BACKEND_SSE2,
    N_BODY_SIM_PRO_SIMD_BACKEND_AVX2,
    N_BODY_SIM_PRO_SIMD_BACKEND_AVX512,
    N_BODY_SIM_PRO_SIMD_BACKEND_NEON
} NBodySimProSimdBackend;

/* Human-readable backend name. Never returns NULL. */
const char* n_body_sim_pro_simd_backend_string(NBodySimProSimdBackend backend);

/*
 * The best backend for which a real kernel exists, given the detected
 * features. Currently: AVX2 (when both AVX2 and FMA are present), else
 * scalar. SSE2/AVX-512/NEON kernels are planned but not yet implemented;
 * they are never selected until they exist.
 */
NBodySimProSimdBackend n_body_sim_pro_simd_best_available_backend(const NBodySimProCpuFeatures* features);

#ifdef __cplusplus
}
#endif

#endif /* N_BODY_SIM_PRO_SIMD_BACKEND_H */
