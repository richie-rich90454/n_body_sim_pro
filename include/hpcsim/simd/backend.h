#ifndef HPCSIM_SIMD_BACKEND_H
#define HPCSIM_SIMD_BACKEND_H

#include "hpcsim/simd/cpu.h"

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

typedef enum HpcsimSimdBackend {
    HPCSIM_SIMD_BACKEND_SCALAR = 0,
    HPCSIM_SIMD_BACKEND_SSE2,
    HPCSIM_SIMD_BACKEND_AVX2,
    HPCSIM_SIMD_BACKEND_AVX512,
    HPCSIM_SIMD_BACKEND_NEON
} HpcsimSimdBackend;

/* Human-readable backend name. Never returns NULL. */
const char* hpcsim_simd_backend_string(HpcsimSimdBackend backend);

/*
 * The best backend for which a real kernel exists, given the detected
 * features. Currently: AVX2 (when both AVX2 and FMA are present), else
 * scalar. SSE2/AVX-512/NEON kernels are planned but not yet implemented;
 * they are never selected until they exist.
 */
HpcsimSimdBackend hpcsim_simd_best_available_backend(const HpcsimCpuFeatures* features);

#ifdef __cplusplus
}
#endif

#endif /* HPCSIM_SIMD_BACKEND_H */
