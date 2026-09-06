---
type: Software Architecture
title: Spektrummer architecture
description: Component boundaries and invariants for the Spektrummer spectral synthesizer.
tags: [juce, synthesizer, architecture, realtime]
status: stable
sources:
  - resource: ../../CMakeLists.txt
    title: Spektrummer build definition
  - resource: ../../src/model/SpectralPresetModel.h
    title: Bounded spectral preset contracts
  - resource: ../../src/model/ExpressionProgram.h
    title: Restricted expression contracts
  - resource: ../../src/PluginProcessor.cpp
    title: Audio processor orchestration
---

# Purpose

Spektrummer is a JUCE C++20 synthesizer delivered as VST3 and Standalone. It
accepts MIDI and creates a fixed harmonic spectrum in the frequency domain,
then transforms that spectrum into audio with inverse FFT overlap-add.

# Component boundaries

- `src/model/` owns the editable and validated spectral preset contracts,
  fixed-capacity model validation, checked musical helpers, and restricted
  expression compilation/evaluation. It is JUCE-independent and does not
  render audio, publish processor state, persist presets, or provide controls.
- `src/dsp/` owns the current fixed-harmonic synthesis, FFT, MIDI decoding data,
  voice state, and ADSR progression. It has no editor dependency and does not
  yet consume the spectral preset model.
- `PluginProcessor` owns JUCE bus configuration, APVTS parameters and state,
  sample-offset MIDI dispatch, output gain, mono/stereo publication, and the
  analyzer FIFO. Issue #6 adds no spectral-preset persistence or audio-thread
  publication to the processor.
- `src/ui/` owns the frequency-domain analyzer display and time-domain ADSR
  preview. UI components read processor-owned state only on the message thread;
  spectral-model editor controls remain deferred.
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
off the audio thread. Draft validation and expression compilation also belong
off the audio thread; issue #6 does not publish the validated model into the
callback. See [Analyzer and real-time constraints](analyzer-and-realtime.md).

# Related concepts

Follow the [bounded spectral preset model](spectral-preset-model.md),
[spectral signal flow](signal-flow.md), [voice lifecycle](voices-and-envelopes.md),
and [parameter contracts](parameters.md) for contract and behavioral detail.
