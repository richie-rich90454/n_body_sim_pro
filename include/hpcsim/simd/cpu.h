#ifndef HPCSIM_SIMD_CPU_H
#define HPCSIM_SIMD_CPU_H

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

typedef struct HpcsimCpuFeatures {
    int has_sse2;
    int has_sse3;
    int has_avx;
    int has_avx2;
    int has_avx512_foundation;
    int has_fma;
    int has_neon;
} HpcsimCpuFeatures;

/* Detect the features of the CPU the program is running on. */
HpcsimCpuFeatures hpcsim_cpu_detect_features(void);

/* Human-readable short CPU name, best-effort. Never returns NULL. */
const char* hpcsim_cpu_brand_string(void);

#ifdef __cplusplus
}
#endif

#endif /* HPCSIM_SIMD_CPU_H */
