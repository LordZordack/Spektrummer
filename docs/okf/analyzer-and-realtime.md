---
type: Realtime Constraints
title: Analyzer and real-time constraints
description: Audio-thread safety rules and the bounded post-gain spectrum transport.
tags: [realtime, analyzer, threading, safety]
status: stable
sources:
  - resource: ../../src/analyzer/AnalyzerSampleFifo.h
    title: Analyzer sample FIFO
  - resource: ../../src/ui/SpectrumComponent.cpp
    title: Spectrum display
  - resource: ../../src/ui/EnvelopeComponent.cpp
    title: Envelope preview
---

# Audio-thread contract

The callback clears and renders caller-provided buffers using fixed-capacity
engine storage. It may perform arithmetic, bounded array traversal, relaxed
atomic parameter loads, and lock-free analyzer writes. It must not allocate,
lock, block, log, access files or networks, touch UI state, or call APVTS
`copyState()` or `replaceState()`.

Prepare and reset own lifecycle operations outside normal processing. They
clear voice, FFT, overlap, gain-ramp, and analyzer-generation state so repeated
use is deterministic across sample rates, zero blocks, varying blocks, and
block partitions.

# Frequency analyzer

The processor publishes post-output-gain mono samples into a bounded
single-producer/single-consumer FIFO. The editor drains this transport on its
timer, computes a magnitude FFT, and displays approximately 20 Hz to Nyquist on
a logarithmic x-axis with a -96 to 0 dBFS y-range. Starting a new processor
generation invalidates old FIFO data without locking the audio callback.

# Envelope preview

The time-domain preview is editor-only. A 60 Hz message-thread timer reads the
four APVTS parameter atomics, updates cached display values, and repaints when
they change. The audio thread neither calls the component nor receives UI
state. Preview geometry clamps non-finite and out-of-range display values so
all plotted points remain finite.

# Channel behavior

The spectral engine renders one mono signal. Output gain advances once per
sample, analyzer publication uses that same sample, and stereo channel two is a
sample-identical copy. Supported mono and stereo instances therefore share the
same automation trajectory and output values.

# Related concepts

See [Architecture](architecture.md), [Spectral signal flow](signal-flow.md), and
[Parameters and state](parameters.md).
