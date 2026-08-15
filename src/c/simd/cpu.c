#include "hpcsim/simd/cpu.h"

#include <string.h>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <cpuid.h>
#endif

static void detect_x86_features(HpcsimCpuFeatures* features) {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    unsigned int maximum_function = __get_cpuid_max(0, NULL);
    if (maximum_function < 1) {
        return;
    }
    unsigned int register_eax = 0;
    unsigned int register_ebx = 0;
    unsigned int register_ecx = 0;
    unsigned int register_edx = 0;
    __cpuid(1, register_eax, register_ebx, register_ecx, register_edx);
    features->has_sse2 = (register_edx >> 26) & 1;
    features->has_sse3 = (register_ecx >> 0) & 1;
    features->has_fma = (register_ecx >> 12) & 1;
    features->has_avx = (register_ecx >> 28) & 1;

    if (maximum_function >= 7) {
        __cpuid_count(7, 0, register_eax, register_ebx, register_ecx, register_edx);
        features->has_avx2 = (register_ebx >> 5) & 1;
        features->has_avx512_foundation = (register_ebx >> 16) & 1;
    }
#else
    (void)features;
#endif
}

static void detect_arm_features(HpcsimCpuFeatures* features) {
#if defined(__aarch64__) || defined(_M_ARM64)
    features->has_neon = 1;
#else
    (void)features;
#endif
}

HpcsimCpuFeatures hpcsim_cpu_detect_features(void) {
    HpcsimCpuFeatures features;
    memset(&features, 0, sizeof(features));
    detect_x86_features(&features);
    detect_arm_features(&features);
    return features;
}

const char* hpcsim_cpu_brand_string(void) {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    static char brand[49];
    unsigned int registers[4];
    const unsigned int maximum_extended = __get_cpuid_max(0x80000000u, NULL);
    if (maximum_extended >= 0x80000004u) {
        __cpuid(0x80000002u, registers[0], registers[1], registers[2], registers[3]);
        memcpy(brand, registers, 16);
        __cpuid(0x80000003u, registers[0], registers[1], registers[2], registers[3]);
        memcpy(brand + 16, registers, 16);
        __cpuid(0x80000004u, registers[0], registers[1], registers[2], registers[3]);
        memcpy(brand + 32, registers, 16);
        brand[48] = '\0';
        return brand;
    }
    return "x86 CPU";
#else
    return "ARM CPU";
#endif
}
