# Porting Checklist

- [ ] Files compile under C++20 with `-Wall -Wextra -Wpedantic`.
- [ ] No `new`/`delete` or heap allocations in `processBlock`.
- [ ] No mutex/lock on the audio thread; use lock-free or atomic flags.
- [ ] Parameter smoothing implemented for audible params.
- [ ] SIMD paths guarded with scalar fallback.
- [ ] Handle NaN/Inf inputs gracefully.
- [ ] Avoid large object copies in the audio loop (use `std::span`/views where possible).
- [ ] Unit or golden tests for filters/FFT where feasible.