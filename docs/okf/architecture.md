---
type: Software Architecture
title: Spektrummer architecture
description: Component boundaries and invariants for the Spektrummer spectral synthesizer.
tags: [juce, synthesizer, architecture, realtime]
status: stable
sources:
  - resource: ../../CMakeLists.txt
    title: Spektrummer build definition
  - resource: ../../src/PluginProcessor.cpp
    title: Audio processor orchestration
---

# Purpose

Spektrummer is a JUCE C++20 synthesizer delivered as VST3 and Standalone. It
accepts MIDI and creates a fixed harmonic spectrum in the frequency domain,
then transforms that spectrum into audio with inverse FFT overlap-add.

# Component boundaries

- `src/dsp/` owns synthesis, FFT, MIDI decoding data, voice state, and ADSR
  progression. It has no editor dependency.
- `PluginProcessor` owns JUCE bus configuration, APVTS parameters and state,
  sample-offset MIDI dispatch, output gain, mono/stereo publication, and the
  analyzer FIFO.
- `src/ui/` owns the frequency-domain analyzer display and time-domain ADSR
  preview. UI components read processor-owned state only on the message thread.
- `PluginEditor` composes controls and APVTS attachments. It does not feed UI
  state into DSP.
- `tests/` exercises DSP, processor, state, MIDI, analyzer transport, and editor
  geometry through the JUCE unit-test executable.

# Stable invariants

The product identity, bundle ID, manufacturer and plug-in codes, VST3 and
Standalone format scope, output-only mono/stereo layouts, and absence of MIDI
output remain stable. Generated files stay below `build/` and outside Git.

The audio callback performs no allocation, locking, blocking, logging, file or
network I/O, or UI access. APVTS `copyState()` and `replaceState()` are used only
off the audio thread. See [Analyzer and real-time constraints](analyzer-and-realtime.md).

# Related concepts

Follow the [spectral signal flow](signal-flow.md), [voice lifecycle](voices-and-envelopes.md),
and [parameter contracts](parameters.md) for behavioral detail.
