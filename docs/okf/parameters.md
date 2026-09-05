---
type: Parameter Reference
title: Parameters and state
description: Stable automatable parameter IDs, ranges, defaults, and restoration rules.
tags: [apvts, parameters, automation, state]
status: stable
sources:
  - resource: ../../src/PluginProcessor.cpp
    title: APVTS parameter and state implementation
---

# Parameter contracts

| ID | Display name | Range | Default | Unit / semantics |
| --- | --- | --- | --- | --- |
| `outputLevel` | Output Level | -60 to 0 | -12 | dB; -60 is exact mute after the 20 ms gain ramp |
| `maxVoices` | Maximum Voices | 2, 4, 8, 16 | 8 | Choice parameter stored as indexes 0 through 3 |
| `attack` | Attack | 0 to 5 | 0.010 | Seconds; zero transitions immediately |
| `decay` | Decay | 0 to 5 | 0.100 | Seconds; zero transitions immediately |
| `sustain` | Sustain | 0 to 1 | 0.800 | Normalized held amplitude |
| `release` | Release | 0 to 10 | 0.080 | Seconds; zero ends the voice on the next rendered sample |

All IDs use parameter version 1 and are stable for host automation and saved
sessions. Time parameters use 1 ms intervals and sustain uses 0.001 intervals.

# Runtime application

The processor caches APVTS raw parameter atomics during construction. The audio
callback reads them with relaxed atomic loads and sends a small target value
object to the engine; it never copies the APVTS state. Output Level and ADSR
control changes use independent 20 ms linear ramps. The engine advances the
shared ADSR control ramp once per sample, then applies the same settings to all
voices. Prepare and reset snap current values to their requested targets;
restored values retarget safely on the next audio callback.

# Serialization and restoration

`getStateInformation` serializes the APVTS tree off the audio thread. Restore
accepts only a `PARAMETERS` tree containing exactly one finite, in-range value
for each of the six stable IDs. Unknown, missing, duplicate, non-finite,
fractional choice, or out-of-range values reject the whole state without
partially changing current parameters.

After valid restoration, the next audio callback synchronizes ADSR and voice
limit values from the atomics. Output gain retargets through its normal ramp.
Prepare and reset also synchronize the cached DSP settings deterministically.

# Related concepts

See [Voices and ADSR envelopes](voices-and-envelopes.md), [Build and use](build-and-use.md),
and [Analyzer and real-time constraints](analyzer-and-realtime.md).
