# Gravity model

## Equations

The engine solves Newtonian gravity in three dimensions. The acceleration
on particle *i* from particle *j* is:

```
a_i = G * m_j * (r_j - r_i) / |r_j - r_i|^3
```

with *G* the gravitational constant, *m_j* the source mass, and the vector
*d = r_j - r_i* the displacement from *i* to *j*.

## Softening

Direct use of the 1/r² force is singular at contact. The engine uses
Plummer softening:

```
a_i = G * m_j * d / (|d|^2 + epsilon^2)^(3/2)
```

where *epsilon* is the softening length. Softening caps the force at small
separations, which prevents the singularity from ejecting particles to
infinity, and improves energy conservation for collisionless systems. With
*epsilon = 0* the model is exactly Newtonian.

The softened pair potential, used by the energy diagnostics, is:

```
U_ij = -G * m_i * m_j / sqrt(|r_j - r_i|^2 + epsilon^2)
```

## Reference kernel

`n_body_sim_pro_gravity_compute_acceleration_reference` computes the exact pairwise
sum above in scalar double precision, single-threaded. It is the
correctness authority for every optimized kernel:

- **OpenMP** parallelizes the outer particle loop; each particle's sum is
  unchanged, so results are bit-identical on a given machine.
- **AVX2** vectorizes the inner loop over source particles with fused
  multiply-add. Lane-wise accumulation reorders the sum, so results agree
  within floating-point tolerance (validated at 1e-10 relative in the test
  suite), not bit-for-bit.
- **Barnes-Hut** replaces distant pairwise sums with single center-of-mass
  interactions (see below).

## Barnes-Hut approximation

Barnes-Hut groups particles into an octree. A cell with center of mass *C*,
total mass *M*, and side length *s* replaces the sum over its members when,
for the query particle at distance *d* from *C*:

```
s / d < theta
```

The opening angle *theta* controls the trade-off. The test suite measures it
directly: at theta = 0.1 the force error is ~1e-5 (RMS, relative to the
reference), at theta = 0.3 ~1e-3, at theta = 0.7 ~1.5e-2, and the amount of
work done decreases as theta grows. Default theta is 0.7.

Because each particle accepts a slightly different set of cells, Barnes-Hut
does not satisfy Newton's third law pair by pair; momentum is conserved only
to the approximation error. The diagnostics report this real momentum error
rather than hiding it.
