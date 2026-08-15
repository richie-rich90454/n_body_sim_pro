#include "hpcsim/generation/random.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static uint64_t splitmix64(uint64_t* state) {
    *state += 0x9E3779B97F4A7C15ull;
    uint64_t result = *state;
    result = (result ^ (result >> 30)) * 0xBF58476D1CE4E5B9ull;
    result = (result ^ (result >> 27)) * 0x94D049BB133111EBull;
    return result ^ (result >> 31);
}

static uint64_t rotate_left(uint64_t value, int shift) {
    return (value << shift) | (value >> (64 - shift));
}

void hpcsim_random_init(HpcsimRandomGenerator* generator, uint64_t seed) {
    uint64_t splitmix_state = seed;
    for (int i = 0; i < 4; ++i) {
        generator->state[i] = splitmix64(&splitmix_state);
    }
}

uint64_t hpcsim_random_next_u64(HpcsimRandomGenerator* generator) {
    const uint64_t result = rotate_left(generator->state[1] * 5, 7) * 9;
    const uint64_t temporary = generator->state[1] << 17;

    generator->state[2] ^= generator->state[0];
    generator->state[3] ^= generator->state[1];
    generator->state[1] ^= generator->state[2];
    generator->state[0] ^= generator->state[3];
    generator->state[2] ^= temporary;
    generator->state[3] = rotate_left(generator->state[3], 45);

    return result;
}

double hpcsim_random_next_double(HpcsimRandomGenerator* generator) {
    return (double)(hpcsim_random_next_u64(generator) >> 11) * (1.0 / 9007199254740992.0);
}

double hpcsim_random_next_double_range(HpcsimRandomGenerator* generator,
                                       double minimum, double maximum) {
    return minimum + (maximum - minimum) * hpcsim_random_next_double(generator);
}

double hpcsim_random_next_gaussian(HpcsimRandomGenerator* generator) {
    double u1 = 0.0;
    while (u1 == 0.0) {
        u1 = hpcsim_random_next_double(generator);
    }
    const double u2 = hpcsim_random_next_double(generator);
    return sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
}
