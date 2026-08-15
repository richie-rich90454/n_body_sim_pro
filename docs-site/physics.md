---
title: Physics & Integrators
description: The gravity model, Plummer softening, the kernel hierarchy, integrators, presets, and conservation diagnostics.
---

# Physics

## Gravity model

The engine solves Newtonian gravity in three dimensions. The acceleration on
particle *i* from particle *j* is

$$
\mathbf{a}_i = G\,m_j\,\frac{\mathbf{r}_j - \mathbf{r}_i}{|\mathbf{r}_j - \mathbf{r}_i|^3}
$$

with *G* the gravitational constant, *m_j* the source mass, and
**d** = *r_j* − *r_i* the displacement.

### Softening

Direct use of the 1/r² force is singular at contact. The engine uses
**Plummer softening**:

$$
\mathbf{a}_i = G\,m_j\,\frac{\mathbf{d}}{(|\mathbf{d}|^2 + \epsilon^2)^{3/2}}
$$

where *ε* is the softening length. Softening caps the force at small
separations, which prevents the singularity from ejecting particles to
infinity and improves energy conservation for collisionless systems. With
*ε* = 0 the model is exactly Newtonian.

The softened pair potential used by the energy diagnostics is

$$
U_{ij} = -G\,\frac{m_i\,m_j}{\sqrt{|\mathbf{r}_j - \mathbf{r}_i|^2 + \epsilon^2}}
$$

### Kernel hierarchy

The reference kernel computes the exact pairwise sum above in scalar double
precision, single-threaded. Every optimization is validated against it:

| Kernel | Order | vs reference |
|--------|-------|--------------|
| reference | O(N²) | exact (authority) |
| openmp | O(N²) | bit-identical (same per-particle sum order) |
| avx2 | O(N²) | ≤ 1e-10 relative (vectorized, reordered sum) |
| barnes_hut | O(N log N) | bounded by θ approximation error |
| barnes_hut_avx2 | O(N log N) | ≤ 1e-9 relative to scalar Barnes-Hut |
| distributed | O(N/P log N) | θ-bound vs single-rank (equivalence test) |

## Integrators

| Method | Order | Symplectic | Notes |
|--------|-------|------------|-------|
| Euler | 1 | no | energy drifts secularly; teaching/baseline |
| Leapfrog | 2 | yes | kick–drift–kick; the astrophysical default |
| Velocity Verlet | 2 | yes | algebraically identical to leapfrog for constant dt |

### Euler

$$
\mathbf{v} \leftarrow \mathbf{v} + \mathbf{a}\,\Delta t,\qquad
\mathbf{x} \leftarrow \mathbf{x} + \mathbf{v}\,\Delta t
$$

First-order and non-symplectic: at the regression test's timestep the
relative energy drift after one orbit is ~1%, versus ~1e-5 for the symplectic
methods.

### Leapfrog (kick–drift–kick)

$$
\begin{aligned}
\mathbf{v} &\leftarrow \mathbf{v} + \tfrac{\Delta t}{2}\,\mathbf{a} && \text{half kick}\\
\mathbf{x} &\leftarrow \mathbf{x} + \mathbf{v}\,\Delta t && \text{drift}\\
\mathbf{a} &\leftarrow \mathbf{a}(\mathbf{x}') && \text{recompute forces}\\
\mathbf{v} &\leftarrow \mathbf{v} + \tfrac{\Delta t}{2}\,\mathbf{a} && \text{half kick}
\end{aligned}
$$

Second-order and symplectic: for fixed timestep the energy error is bounded
and oscillatory rather than secular, which is what makes long simulations
stable.

### Velocity Verlet

$$
\mathbf{x}' = \mathbf{x} + \mathbf{v}\,\Delta t + \tfrac{1}{2}\mathbf{a}\,\Delta t^2,\qquad
\mathbf{a}' = \mathbf{a}(\mathbf{x}'),\qquad
\mathbf{v}' = \mathbf{v} + \tfrac{\Delta t}{2}\left(\mathbf{a} + \mathbf{a}'\right)
$$

For a constant timestep this is algebraically identical to kick–drift–kick
leapfrog. The implementation applies the velocity half-kick before the drift,
which reproduces the same trajectory without scratch storage for the old
accelerations.

Forces are passed to the integrator as a callback with a context pointer
(reference, OpenMP, SIMD, Barnes-Hut, or the distributed kernel). Symplectic
steps invoke it twice; the initial accelerations are computed when a preset
is applied.

## Presets

Presets are **simplified educational models**, not scientifically exact
reproductions. Each uses G = 1 and unit masses; length and velocity scales
are order unity. All presets are deterministic: same preset, count, and seed
→ identical initial conditions.

| Preset | Model |
|--------|-------|
| two_body | two equal masses in a circular orbit about their barycenter |
| random_cloud | uniform positions in a unit sphere, small random velocities |
| solar_system | central star + planets on circular orbits |
| open_cluster | Gaussian positions and velocities |
| globular_cluster | Plummer sphere, isotropic velocities |
| spiral_galaxy | exponential disk, flat rotation curve, two spiral arms |
| elliptical_galaxy | Plummer sphere with larger scale radius |
| galaxy_collision | two disks approaching head-on |
| triple_galaxy | three Plummer spheres in a triangle |

### Plummer spheres

Radii come from the analytic profile

$$
r = \frac{a}{\sqrt{u^{-2/3} - 1}},\qquad u \sim U(0,1)
$$

and velocities from an isotropic Gaussian with the Plummer one-dimensional
dispersion

$$
\sigma^2 = \frac{GM}{6\sqrt{a^2 + r^2}}
$$

The mean velocity is removed so the system starts with zero net momentum.

### Disks

Radii are sampled from the exponential surface-density profile — a
Gamma(2, *R_d*) draw — and the tangential velocity is set to a constant
circular speed with a small Gaussian dispersion. A flat rotation curve
implies a dark halo that is not modeled; the preset is an approximation,
documented as such.

### Galaxy collision

The flagship demo: two disks fall together, tidally distort, merge, and
relax into a distorted remnant. The approach speed is chosen so the
interaction happens over a visually interesting timescale.

### Determinism

Every generator uses the engine's xoshiro256** PRNG. The parallel
first-touch generator derives each particle's randomness deterministically
from `(seed, particle index)`, so it is reproducible across any thread
count. The sequential generator (a single PRNG stream) remains the
correctness reference.

## Conservation diagnostics

Per step, the engine computes from the actual particle state:

- kinetic energy $K = \tfrac{1}{2}\sum_i m_i|\mathbf{v}_i|^2$
- total momentum $\mathbf{P} = \sum_i m_i\mathbf{v}_i$
- angular momentum $\mathbf{L} = \sum_i m_i(\mathbf{r}_i \times \mathbf{v}_i)$
- center of mass $\mathbf{R} = \sum_i m_i\mathbf{r}_i / \sum_i m_i$

Momentum error and center-of-mass displacement are reported relative to
their initial values. Energy drift requires the O(N²) potential sum, so it
is tracked (every 60 steps) only for systems with ≤ 20,000 particles; larger
systems report `N/A`.

Because Barnes-Hut and its distributed form break Newton's third law pair by
pair, momentum is conserved only to the approximation error — the UI reports
this real residual rather than hiding it.
