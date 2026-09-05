---
type: MIDI Behavior
title: MIDI behavior
description: MIDI messages recognized by Spektrummer and their deterministic voice effects.
tags: [midi, notes, panic, timing]
status: stable
sources:
  - resource: ../../src/dsp/SynthMidiEvent.h
    title: Bounded MIDI event decoder
  - resource: ../../src/PluginProcessor.cpp
    title: Sample-offset MIDI dispatch
---

# Supported events

- Note-on starts or retriggers one matching channel-and-note voice. Velocity
  scales voice amplitude; velocity zero is treated as note-off.
- Note-off starts release from the voice's current envelope level.
- Controller 123, all-notes-off, releases voices on the message's MIDI channel.
- Controller 120, all-sound-off, is a global panic that clears every voice and
  all pending overlap audio immediately.

Other MIDI messages do not alter synthesis state. Spektrummer accepts MIDI input
and produces no MIDI output.

# Timing

The processor renders up to each event's sample offset, applies the event, and
then continues. Zero-sample callbacks still apply MIDI events, including panic.
Envelope state therefore begins at the host event offset. The next 512-sample
spectral hop determines when the changed voice amplitude becomes audible.

# Polyphony interactions

Repeated channel-and-note note-on uses one voice and restarts it from attack.
At the selected voice limit, a new note steals the oldest active voice. Voice
age is deterministic after prepare and reset. See [Voices and ADSR envelopes](voices-and-envelopes.md).
