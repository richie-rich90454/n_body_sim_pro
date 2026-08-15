#include "n_body_sim_pro/simd/backend.h"

#include <stddef.h>

const char* n_body_sim_pro_simd_backend_string(NBodySimProSimdBackend backend) {
    switch (backend) {
        case N_BODY_SIM_PRO_SIMD_BACKEND_SCALAR:
            return "scalar";
        case N_BODY_SIM_PRO_SIMD_BACKEND_SSE2:
            return "SSE2";
        case N_BODY_SIM_PRO_SIMD_BACKEND_AVX2:
            return "AVX2";
        case N_BODY_SIM_PRO_SIMD_BACKEND_AVX512:
            return "AVX-512";
        case N_BODY_SIM_PRO_SIMD_BACKEND_NEON:
            return "NEON";
    }
    return "unknown";
}

NBodySimProSimdBackend n_body_sim_pro_simd_best_available_backend(const NBodySimProCpuFeatures* features) {
    if (features == NULL) {
        return N_BODY_SIM_PRO_SIMD_BACKEND_SCALAR;
    }
    /* AVX2 kernels require FMA for the fused multiply-add accumulation. */
    if (features->has_avx2 && features->has_fma) {
        return N_BODY_SIM_PRO_SIMD_BACKEND_AVX2;
    }
    return N_BODY_SIM_PRO_SIMD_BACKEND_SCALAR;
}
