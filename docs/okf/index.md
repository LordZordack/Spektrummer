---
okf_version: "0.2"
---

# Spektrummer knowledge bundle

This Open Knowledge Format bundle is the maintained product and engineering
reference for Spektrummer.

## Product and architecture

- [Architecture](architecture.md) - Major components, ownership boundaries, and repository invariants.
- [Spectral signal flow](signal-flow.md) - MIDI-to-audio processing through inverse FFT overlap-add and analysis.
- [Voices and ADSR envelopes](voices-and-envelopes.md) - Voice allocation, lifecycle, envelope timing, and reset semantics.
- [Parameters and state](parameters.md) - Stable APVTS contracts, ranges, defaults, automation, and restoration.
- [MIDI behavior](midi.md) - Supported note and panic messages with sample-offset semantics.
- [Analyzer and real-time constraints](analyzer-and-realtime.md) - Spectrum transport, threading boundaries, and audio-callback rules.

## Guides and direction

- [Build and use Spektrummer](build-and-use.md) - Prerequisites, build commands, plug-in locations, and control workflow.
- [Future envelope models](future-envelope-models.md) - Potential DAHDSR, MSEG, and mathematical envelope capabilities.
- [Bundle update log](log.md) - Dated history of knowledge changes.
