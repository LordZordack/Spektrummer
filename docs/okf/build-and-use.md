---
type: User Guide
title: Build and use Spektrummer
description: Practical setup, build, test, installation, and control workflow.
tags: [build, usage, vst3, standalone]
status: stable
sources:
  - resource: ../../README.md
    title: Spektrummer README
  - resource: ../../CMakePresets.json
    title: Configured CMake presets
---

# Prerequisites

- Visual Studio Community 2026 with Desktop development with C++, MSVC,
  Windows SDK, and CMake tools.
- CMake 4.2 or newer on `PATH`.
- Git with submodule support.

# Build and test

From the Spektrummer repository root:

```powershell
git submodule update --init --recursive
cmake --preset vs2026
cmake --build --preset debug
ctest --preset debug --output-on-failure
cmake --build --preset release
```

Artifacts are written below
`build/vs2026/Spektrummer_artefacts/<configuration>/`. VST3 builds are copied
to `%LOCALAPPDATA%\Programs\Common\VST3` for per-user host discovery. The
Standalone executable is available in the corresponding Standalone directory.

# Use

Load the VST3 in a host or launch Standalone, route MIDI to Spektrummer, and play
notes. Output Level sets final gain. Maximum Voices selects 2, 4, 8, or 16-note
polyphony. Attack, Decay, Sustain, and Release shape each voice; the time-domain
preview reflects their configured shape. The upper spectrum graph displays the
post-gain output magnitude.

For exact parameter values and endpoints, see [Parameters and state](parameters.md).
For retrigger, stealing, and panic semantics, see [MIDI behavior](midi.md).

# Troubleshooting

Initialize the pinned `external/JUCE` submodule before configuring. Keep build
products outside Git. If the VST3 is not discovered, verify the per-user VST3
directory and rescan plug-ins in the host. Build or test failures should be
reported with the exact preset command and first relevant compiler/test error.
