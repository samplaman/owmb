# Fast block synthesizer — work in progress

This directory holds an experimental synthesizer that renders Partials in
fixed-size blocks (frames), interpolating parameters linearly within each
block instead of sample-by-sample as the standard `Synthesizer` does. The
goal is speed: on a distilled clarinet analysis (409 partials) it renders
about **8x faster** than the standard synthesizer, at a measured cost of
about **-55 dB** RMS error relative to it for sinusoidal (non-enhanced)
partials.

**Status: under active development, not ready for general use.** The
driver utility (`utils/loris_fastsynth_main.cpp`, built as
`loris-fastsynth`) and the comparison harness (`test/test_FastSynth.cpp`,
CTest target `test_fastsynth`) are the two ways to exercise this code.
Neither this directory's headers nor `loris-fastsynth` are installed by
the build; nothing here is part of the public Loris API yet.


## What works

- **Sinusoidal (bw = 0) synthesis is correct**, to within the block's
  linear-interpolation approximation. Onsets, glides, and amplitude
  ramps all render with the standard synthesizer's phase to within
  about 0.01-0.05 radians, at a fixed, designed latency of one block.
- The one-block latency is uniform for every Partial, including those
  that start at t = 0: `BlockOscillator::initOnset()` initializes a
  newly-onsetting oscillator directly from its target Breakpoint (phase
  walked back one block), so correct rendering never depends on a
  fade-in Breakpoint having preceded it in the frame stream. (This
  replaced an earlier assumption -- "all Partials fade in" -- that broke
  for Partials starting exactly at t = 0; see git history on
  `BlockSynthBwe.cpp` for the diagnosis.)


## What does not work yet

**Bandwidth-enhanced (noisy) synthesis is not functional.**
`BlockOscillator` has the full amplitude-modulation implementation
(carrier-amplitude and modulation-index lookup tables as functions of
bandwidth, interpolated per sample), but its noise source is a stub:
`generate_randi()` in `BlockSynthBwe.cpp` has its body commented out and
currently just writes zeros. The measured effect: a partial with
bandwidth 0.5 renders at exactly its carrier amplitude (no noise energy
at all), and a partial with bandwidth 1.0 (pure noise) renders as
**silence**.

Re-enabling this is the next real piece of work. Known issues queued up
behind the stub, for whoever picks this up:

1. `BlockSynthBwe::render()` passes the noise buffer's base pointer to
   every oscillator's `oscillate()` call, rather than advancing through
   `NoiseBuffer + mNoiseBufferIndex` per block -- once the generator is
   live, every block would reuse the same 100 noise samples (a periodic
   buzz rather than noise).
2. The `// TODO: need a decorrelating delay here! NOT YET IMPLEMENTED`
   in `BlockSynthBwe::render()`: currently all Partials in a frame would
   share one noise realization coherently, rather than being
   decorrelated from one another.
3. The noise's spectral character (`generate_randi`'s decimated,
   interpolated Gaussian noise plus the commented-out `apply_filter`
   prototype filter) has not been validated against the standard
   engine's `NoiseGenerator` + `Filter` chain -- they may or may not
   sound alike even once both are live.

`test/test_FastSynth.cpp` should grow an energy-vs-bandwidth assertion
(rendering the same bw = 0.0 / 0.5 / 1.0 partial through both engines and
comparing RMS energy) once noise synthesis is reinstated -- that
comparison already exists as an ad hoc measurement in the git history
around the commit that diagnosed this stub.


## History

This is the second implementation of the fast synthesizer. The first was
a vectorized, structure-of-arrays design (`generate_env_segment`,
`generate_table_lookup_01`, and oscillator/multiply-add kernels in
`oscil.c` / `multiplyAdd.c`), superseded by the current per-oscillator
class design (`BlockOscillator`, `BlockSynthBwe`, `BlockSynthReader`) and
removed as dead code in 2026-07 (see the commit that deletes
`fastsynth.h`/`fastsynth.cpp`/`oscil.*`/`multiplyAdd.*` for the full
diagnosis). If that vectorized approach is ever worth a second look, it
is preserved there in git history.

For the full narrative of how this code was diagnosed and fixed --
including the heap-corruption bug the comparison harness caught on day
one, and the phase-offset root cause -- see `PROJECT-STATUS.md` at the
repository root (local working notes, not tracked in git).
