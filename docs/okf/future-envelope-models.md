---
type: Product Direction
title: Future envelope models
description: Candidate envelope systems beyond the current fixed ADSR model.
tags: [future, envelope, dahdsr, mseg, modulation]
status: draft
sources:
  - resource: voices-and-envelopes.md
    title: Current ADSR behavior
---

# Current boundary

Spektrummer currently provides one linear ADSR amplitude model per voice. The
four parameter IDs and their saved-session behavior are stable. Alternative
models must preserve existing sessions, deterministic voice lifecycle,
block-partition invariance, finite output, and audio-thread safety.

# Candidate: DAHDSR

A Delay-Attack-Hold-Decay-Sustain-Release option could support percussion and
sound-design shapes while retaining named stages. A model selector would need
a stable parameter contract, deterministic zero-time transitions, and explicit
retrigger and note-off behavior during delay and hold.

# Candidate: MSEG

A multi-stage envelope generator could represent arbitrary breakpoint sequences,
curves, loops, and tempo synchronization. Its state format must be bounded or
compiled off the audio thread into fixed-capacity runtime data. Voice stealing,
loop exit on note-off, host-tempo discontinuities, and automation require tests
before exposing the model.

# Candidate: mathematical functions

User-supplied mathematical envelope functions could map normalized stage time
and parameters to amplitude. Raw interpretation, dynamic allocation, or script
execution is unsuitable for the audio callback. A safe design would validate
and compile a restricted expression off-thread, publish an immutable bounded
representation, define finite-domain behavior, and reject non-finite output.

# Evaluation criteria

Any future model should document and test parameter/state migration, stage and
retrigger semantics, reset, voice stealing, panic, sample-rate changes, block
partitions, finite endpoints, CPU bounds, and preview parity. See
[Analyzer and real-time constraints](analyzer-and-realtime.md) and
[Parameters and state](parameters.md).
