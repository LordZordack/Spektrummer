---
type: DSP Behavior
title: Voices and ADSR envelopes
description: Deterministic voice allocation and amplitude-envelope lifecycle semantics.
tags: [voices, polyphony, adsr, midi]
status: stable
sources:
  - resource: ../../src/dsp/SpectralSynthEngine.h
    title: Voice and envelope declarations
  - resource: ../../src/dsp/SpectralSynthEngine.cpp
    title: Voice and envelope implementation
---

# Voice allocation

The engine has fixed storage for 16 voices and permits maximum polyphony of 2,
4, 8, or 16. Note-on reuses an existing voice with the same MIDI channel and
note; otherwise it selects an inactive slot. At the active limit, it steals the
oldest voice. Lowering the limit also removes oldest voices until the count is
valid. No allocation occurs during these operations.

Velocity is clamped to 0 through 1. A note-on with zero velocity follows MIDI
convention and behaves as note-off. A positive retrigger clears the matching
voice's phase and envelope state, assigns it a new age, and begins attack from
zero.

# ADSR lifecycle

- **Attack** progresses linearly from 0 to 1 over the configured attack time.
- **Decay** progresses linearly from 1 to the configured sustain level.
- **Sustain** holds the configured normalized level while the note is held.
- **Release** begins at the voice's current level and progresses linearly to 0.
  The voice becomes inactive when release completes.

State advances once per rendered sample and is independent of host block
partitioning. The frequency-domain frame samples each voice level at the next
512-sample hop, as described in [Spectral signal flow](signal-flow.md).
Automated ADSR control values move toward their targets on a 20 ms linear ramp,
advanced once per sample and shared consistently by all voices.

Zero attack or decay transitions immediately to the following stage. Zero
release makes the voice inactive at the next rendered sample. Sustain zero is
a valid silent held voice. Parameter values are sanitized to finite supported
ranges before use.

# MIDI and lifecycle events

Note-off during attack, decay, or sustain captures the current level and starts
release. Repeated note-off while already releasing does not restart the tail.
All-notes-off releases voices matching its channel, or all channels when the
engine is called with channel zero. All-sound-off is global: it clears voices,
spectrum state, and pending overlap samples immediately.

`prepare` rebuilds sample-rate-dependent spectral kernels and then resets all
runtime state. `reset` clears voices, phases, spectrum, overlap, frame timing,
and voice ages deterministically while retaining the configured parameters and
voice limit.

# Related concepts

See [MIDI behavior](midi.md), [Parameters and state](parameters.md), and
[Future envelope models](future-envelope-models.md).
