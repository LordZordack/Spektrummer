# Spektrummer

Spektrummer is a JUCE synthesizer whose sound engine will create spectral
content in the frequency domain and transform it into time-domain audio. The
name reflects that spectral core.

## Current milestone

Version 0.1.0 is a safe, testable project foundation. It builds as VST3 and
Standalone, accepts MIDI, supports mono or stereo output, and deliberately
emits silence until the synthesis architecture is designed and implemented.

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

Later milestones will define the frequency-domain synthesis model and add
polyphonic MIDI voices, velocity response, output volume, parameter state, and
a live output spectral analyzer. Before that work starts, the design must fix
the FFT size and overlap strategy, phase handling, voice allocation and
stealing, envelope behaviour, analyzer data transport, parameter set, and
latency policy.

The project intentionally remains VST3/Standalone-only and does not enable web
browser, curl, MIDI output, distribution tooling, or additional plug-in
formats.
