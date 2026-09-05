# Spektrummer

Spektrummer is a JUCE synthesizer whose sound engine will create spectral
content in the frequency domain and transform it into time-domain audio. The
name reflects that spectral core.

## Current milestone

Version 0.1.0 is a playable spectral-synthesis MVP. It builds as VST3 and
Standalone, accepts MIDI, and generates a fixed harmonic spectrum through a
shared inverse-FFT overlap-add engine. The output supports mono or stereo,
with the same synthesized signal on both stereo channels.

The editor provides a smoothed output-level control, four ADSR controls, a
compact envelope preview, selectable maximum polyphony of 2, 4, 8, or 16
voices, and a live post-output-gain magnitude spectrum. The spectrum spans
approximately 20 Hz to Nyquist on a logarithmic frequency axis and clamps its
display to -96 through 0 dBFS.

For the MVP, note-on velocity controls voice level and repeated notes retrigger
their matching voice from zero. Each voice owns an Attack, Decay, Sustain, and
Release envelope. Envelope state advances per sample; its level is sampled by
the spectral engine at each 512-sample synthesis hop. MIDI state changes are
applied at their exact host sample offset, while audible spectral changes occur
on the next hop. Note-off releases from the current envelope level. New notes
steal the oldest active voice at the selected limit. All-notes-off releases
matching channel voices, while all-sound-off acts as a global panic that clears
every voice and pending overlap sample immediately.

The ADSR parameter defaults are 10 ms attack, 100 ms decay, 0.8 sustain, and
80 ms release. Attack and decay range from 0 to 5 seconds, sustain from 0 to 1,
and release from 0 to 10 seconds. Zero-time stages transition immediately.

## Identity

| Field | Value |
| --- | --- |
| Target and product | `Spektrummer` |
| Company | `LordZordack` |
| Bundle ID | `com.lordzordack.spektrummer` |
| Manufacturer / plug-in codes | `LZrd` / `Spkt` |
| Version | `0.1.0` |

Treat the bundle ID and four-character codes as stable after using builds in
saved host sessions.

## Prerequisites

- Visual Studio Community 2026 with Desktop development with C++, MSVC,
  Windows SDK, and CMake tools.
- CMake 4.2 or newer on `PATH`.
- Git with submodule support.

## Build and test

Run from the Spektrummer repository root:

```powershell
git submodule update --init --recursive
cmake --preset vs2026
cmake --build --preset debug
ctest --preset debug --output-on-failure
cmake --build --preset release
```

Build products appear under
`build/vs2026/Spektrummer_artefacts/<configuration>/`. VST3 builds are copied
to `%LOCALAPPDATA%\Programs\Common\VST3` for per-user host discovery.

## Product knowledge

The version-controlled [Open Knowledge Format bundle](docs/okf/index.md) is the
entry point for Spektrummer architecture, signal flow, voice and envelope
semantics, parameters, MIDI behavior, analyzer constraints, build/use guidance,
and future ideas. Contributors and agents should update affected concepts and
the bundle log whenever product behavior changes.

## Planned product direction

Later milestones can expand the fixed harmonic model with editable spectral
content, alternative envelopes, modulation, filtering, spatialization, and richer MIDI
performance behavior. The current engine intentionally keeps those choices
fixed so the inverse-FFT synthesis, phase handling, overlap reconstruction,
voice stealing, parameter state, and real-time-safe analyzer transport remain
small and testable.

The project intentionally remains VST3/Standalone-only and does not enable web
browser, curl, MIDI output, distribution tooling, or additional plug-in
formats.
