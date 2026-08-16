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
    /* Each backend is only returned when the CPU has the ISA *and* a real
     * kernel was compiled in (N_BODY_SIM_PRO_HAVE_*_KERNEL is defined by CMake
     * when the matching translation unit is built with the ISA's flags). The
     * FMA check matters because the kernels use fused multiply-add. */
#ifdef N_BODY_SIM_PRO_HAVE_AVX512_KERNEL
    if (features->has_avx512_foundation && features->has_fma) {
        return N_BODY_SIM_PRO_SIMD_BACKEND_AVX512;
    }
#endif
#ifdef N_BODY_SIM_PRO_HAVE_AVX2_KERNEL
    if (features->has_avx2 && features->has_fma) {
        return N_BODY_SIM_PRO_SIMD_BACKEND_AVX2;
    }
#endif
#ifdef N_BODY_SIM_PRO_HAVE_NEON_KERNEL
    /* NEON is always present on AArch64; its kernels exist there. */
    if (features->has_neon) {
        return N_BODY_SIM_PRO_SIMD_BACKEND_NEON;
    }
#endif
    return N_BODY_SIM_PRO_SIMD_BACKEND_SCALAR;
}
