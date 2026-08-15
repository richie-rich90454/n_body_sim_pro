# Presets

The initial-condition presets are **simplified educational models**, not
scientifically exact astronomical reproductions. Each uses G = 1 and unit
masses; length and velocity scales are of order unity. All presets are
deterministic: the same preset, particle count, and seed produce identical
initial conditions (validated by the test suite).

| Preset | Model |
|--------|-------|
| two_body | Two equal masses on a circular orbit about their barycenter. |
| random_cloud | Uniform positions in a unit sphere, small random velocities. |
| solar_system | A central unit-mass star plus planets on circular orbits with increasing radii. |
| open_cluster | Positions and velocities drawn from Gaussians (a loose, unbound-by-construction cluster). |
| globular_cluster | Plummer sphere with isotropic Gaussian velocities from the analytic velocity dispersion. |
| spiral_galaxy | Exponential disk with a flat rotation curve, small velocity dispersion, and two trailing spiral arms. |
| elliptical_galaxy | Plummer sphere with a larger scale radius than the globular cluster. |
| galaxy_collision | Two exponential disks approaching head-on along the x axis at equal and opposite speeds. |
| triple_galaxy | Three Plummer spheres arranged in a triangle. |

## Notes on the models

- **Plummer spheres** sample radii from the analytic profile
  `r = a / sqrt(u^(-2/3) - 1)` and velocities from an isotropic Gaussian
  with the Plummer one-dimensional dispersion
  `sigma^2 = G M / (6 sqrt(a^2 + r^2))`. The mean velocity is removed so the
  system starts with zero net momentum. This produces a near-equilibrium
  cluster.
- **Disks** sample radii from the exponential surface-density profile
  (a Gamma(2, R_d) draw) and set the tangential velocity to a constant
  circular speed with a small Gaussian dispersion. A flat rotation curve
  implies a dark halo that is not modeled; the preset is an approximation,
  which is fine for an educational tool and documented as such.
- **Galaxy collision** is the flagship demo: two disks fall together, tidally
  distort, merge, and relax into a distorted remnant. The approach speed is
  chosen so the interaction happens over a visually interesting timescale.

Every generator removes the mean velocity of each generated block so
initial net momentum is zero (the test suite asserts this). Seeding uses the
engine's xoshiro256** generator, so runs are reproducible.
