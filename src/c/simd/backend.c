#include "hpcsim/simd/backend.h"

#include <stddef.h>

const char* hpcsim_simd_backend_string(HpcsimSimdBackend backend) {
    switch (backend) {
        case HPCSIM_SIMD_BACKEND_SCALAR:
            return "scalar";
        case HPCSIM_SIMD_BACKEND_SSE2:
            return "SSE2";
        case HPCSIM_SIMD_BACKEND_AVX2:
            return "AVX2";
        case HPCSIM_SIMD_BACKEND_AVX512:
            return "AVX-512";
        case HPCSIM_SIMD_BACKEND_NEON:
            return "NEON";
    }
    return "unknown";
}

HpcsimSimdBackend hpcsim_simd_best_available_backend(const HpcsimCpuFeatures* features) {
    if (features == NULL) {
        return HPCSIM_SIMD_BACKEND_SCALAR;
    }
    /* AVX2 kernels require FMA for the fused multiply-add accumulation. */
    if (features->has_avx2 && features->has_fma) {
        return HPCSIM_SIMD_BACKEND_AVX2;
    }
    return HPCSIM_SIMD_BACKEND_SCALAR;
}
