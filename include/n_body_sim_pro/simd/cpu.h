#ifndef N_BODY_SIM_PRO_SIMD_CPU_H
#define N_BODY_SIM_PRO_SIMD_CPU_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Runtime CPU feature detection.
 *
 * The engine never assumes every CPU supports every backend. Detection runs
 * at startup (once, cached by the caller) and selects the best kernel the
 * hardware actually supports.
 */

typedef struct NBodySimProCpuFeatures {
    int has_sse2;
    int has_sse3;
    int has_avx;
    int has_avx2;
    int has_avx512_foundation;
    int has_fma;
    int has_neon;
} NBodySimProCpuFeatures;

/* Detect the features of the CPU the program is running on. */
NBodySimProCpuFeatures n_body_sim_pro_cpu_detect_features(void);

/* Human-readable short CPU name, best-effort. Never returns NULL. */
const char* n_body_sim_pro_cpu_brand_string(void);

#ifdef __cplusplus
}
#endif

#endif /* N_BODY_SIM_PRO_SIMD_CPU_H */
