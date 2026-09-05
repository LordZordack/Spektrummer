---
type: Signal Flow
title: Spectral signal flow
description: How MIDI and parameters become synthesized audio and analyzer data.
tags: [dsp, fft, overlap-add, signal-flow]
status: stable
sources:
  - resource: ../../src/dsp/SpectralSynthEngine.cpp
    title: Spectral synthesis engine
  - resource: ../../src/PluginProcessor.cpp
    title: Processor audio callback
---

# Processing sequence

1. `PluginProcessor::processBlock` clears the output-only host buffer and reads
   cached APVTS atomics for maximum voices, ADSR settings, and output level.
2. MIDI events are dispatched at their host-provided sample offsets. Rendering
   is split at each event, so [note and panic state](midi.md) changes at the
   correct sample.
3. Each active voice advances its [ADSR state](voices-and-envelopes.md) once per
   rendered sample.
4. Every 512 samples, `SpectralSynthEngine` builds a spectrum for the active
   voices. Eight harmonics use inverse-harmonic weighting and precomputed Hann
   kernels; each voice's current envelope level and velocity scale its bins.
5. A 2048-point inverse FFT converts the spectrum to a real frame. Frames are
   accumulated into the circular overlap buffer at a 512-sample hop.
6. The processor applies the shared 20 ms output-gain ramp to the mono result.
7. Post-gain mono samples are pushed to the bounded analyzer FIFO and copied
   sample-identically to the second channel for stereo output.

# Timing consequences

MIDI and ADSR state are sample-accurate, but spectral amplitude changes become
audible at the next synthesis hop. A note-on starts at envelope level zero, so
the initial hop is silent before the first nonzero spectral frame. Old
overlap-add contributions can remain audible after a voice reaches inactive;
all-sound-off is the exception because it clears pending overlap immediately.

# Related concepts

See [Architecture](architecture.md), [Parameters and state](parameters.md), and
[Analyzer and real-time constraints](analyzer-and-realtime.md).
