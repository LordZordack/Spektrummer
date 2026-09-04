# Spektrummer

Spektrummer is a JUCE synthesizer whose sound engine will create spectral
content in the frequency domain and transform it into time-domain audio. The
name reflects that spectral core.

## Current milestone

Version 0.1.0 is a playable spectral-synthesis MVP. It builds as VST3 and
Standalone, accepts MIDI, and generates a fixed harmonic spectrum through a
shared inverse-FFT overlap-add engine. The output supports mono or stereo,
with the same synthesized signal on both stereo channels.

The editor provides a smoothed output-level control, selectable maximum
polyphony of 2, 4, 8, or 16 voices, and a live post-output-gain magnitude
spectrum. The graph spans approximately 20 Hz to Nyquist on a logarithmic
frequency axis and clamps its display to -96 through 0 dBFS.

For the MVP, note-on velocity controls voice level, repeated notes retrigger
their matching voice, and the Hann-windowed synthesis frame supplies a smooth
onset. MIDI state changes are applied at their exact host sample offset; audible
spectral changes occur on the next 512-sample synthesis hop. Note-off uses an
80 ms hop-stepped release, including for very short notes. New notes steal the
oldest active voice at the selected limit. All-notes-off releases matching
voices, while all-sound-off acts as a global panic that clears every voice and
pending overlap sample immediately.

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

## Planned product direction

Later milestones can expand the fixed harmonic model with editable spectral
content, envelopes, modulation, filtering, spatialization, and richer MIDI
performance behavior. The current engine intentionally keeps those choices
fixed so the inverse-FFT synthesis, phase handling, overlap reconstruction,
voice stealing, parameter state, and real-time-safe analyzer transport remain
small and testable.

The project intentionally remains VST3/Standalone-only and does not enable web
browser, curl, MIDI output, distribution tooling, or additional plug-in
formats.
