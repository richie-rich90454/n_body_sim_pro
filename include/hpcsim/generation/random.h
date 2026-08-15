#ifndef HPCSIM_GENERATION_RANDOM_H
#define HPCSIM_GENERATION_RANDOM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Deterministic pseudo-random number generator for particle generation.
 *
 * Uses xoshiro256** (Blackman & Vigna), a fast, high-quality generator whose
 * state fits in 32 bytes. Every preset must be reproducible from its seed,
 * so generation never uses the C library rand() or platform randomness.
 */

typedef struct HpcsimRandomGenerator {
    uint64_t state[4];
} HpcsimRandomGenerator;

/* Seed the generator from `seed` (seeds are mixed with splitmix64). */
void hpcsim_random_init(HpcsimRandomGenerator* generator, uint64_t seed);

/* Uniform 64-bit integer in [0, 2^64). */
uint64_t hpcsim_random_next_u64(HpcsimRandomGenerator* generator);

/* Uniform double in [0, 1). */
double hpcsim_random_next_double(HpcsimRandomGenerator* generator);

/* Uniform double in [minimum, maximum). */
double hpcsim_random_next_double_range(HpcsimRandomGenerator* generator,
                                       double minimum, double maximum);

/* Gaussian (normal) sample with mean 0 and unit variance (Box-Muller). */
double hpcsim_random_next_gaussian(HpcsimRandomGenerator* generator);

#ifdef __cplusplus
}
#endif

#endif /* HPCSIM_GENERATION_RANDOM_H */
