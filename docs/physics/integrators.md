# Integrators

Three integrators are implemented. Their numerical properties are validated
by the two-body regression test, which runs a circular orbit for eight
orbital periods and checks separation, energy, momentum, and center of mass.

## Euler (explicit, first order)

```
x <- x + v dt
v <- v + a dt
```

First-order and non-symplectic. Energy drifts secularly: after one orbit the
relative energy drift is on the order of a percent at the test timestep,
versus ~1e-5 for the symplectic methods. Useful as a baseline and for
teaching; not an astrophysical default.

## Leapfrog (kick-drift-kick, second order, symplectic)

```
v <- v + (dt/2) a        half kick
x <- x + v dt            drift
a <- a(x')               recompute forces
v <- v + (dt/2) a        half kick
```

Second-order and symplectic: for fixed timestep the energy error is bounded
and oscillatory rather than secular, which is what makes long simulations
stable. This is the astrophysical default.

## Velocity Verlet (second order, symplectic)

Classic velocity Verlet is

```
x' = x + v dt + (1/2) a dt^2
a' = a(x')
v' = v + (1/2)(a + a') dt
```

For a constant timestep this is algebraically identical to the
kick-drift-kick leapfrog. The implementation applies the velocity half-kick
before the drift, which reproduces the same trajectory without needing
scratch storage for the old accelerations. Both are kept as distinct
functions because the two orderings are what people expect to find and
compare.

## Timestep and forces

The force kernel is passed to the integrator as a callback with a context
pointer (reference, OpenMP, SIMD, or Barnes-Hut). Symplectic steps invoke it
twice: once after the drift. The initial accelerations must be computed
before the first step; the simulation controller does this when a preset is
applied.
