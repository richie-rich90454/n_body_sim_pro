#ifndef N_BODY_SIM_PRO_GENERATION_RANDOM_H
#define N_BODY_SIM_PRO_GENERATION_RANDOM_H

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

typedef struct NBodySimProRandomGenerator {
    uint64_t state[4];
} NBodySimProRandomGenerator;

/* Seed the generator from `seed` (seeds are mixed with splitmix64). */
void n_body_sim_pro_random_init(NBodySimProRandomGenerator* generator, uint64_t seed);

/* Uniform 64-bit integer in [0, 2^64). */
uint64_t n_body_sim_pro_random_next_u64(NBodySimProRandomGenerator* generator);

/* Uniform double in [0, 1). */
double n_body_sim_pro_random_next_double(NBodySimProRandomGenerator* generator);

/* Uniform double in [minimum, maximum). */
double n_body_sim_pro_random_next_double_range(NBodySimProRandomGenerator* generator,
                                       double minimum, double maximum);

/* Gaussian (normal) sample with mean 0 and unit variance (Box-Muller). */
double n_body_sim_pro_random_next_gaussian(NBodySimProRandomGenerator* generator);

#ifdef __cplusplus
}
#endif

#endif /* N_BODY_SIM_PRO_GENERATION_RANDOM_H */
