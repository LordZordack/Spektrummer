---
type: Data Model
title: Bounded spectral preset model
description: Editable spectral definitions, bounded validation, and restricted expression contracts.
tags: [spectral-model, validation, expressions, realtime]
status: stable
sources:
  - resource: ../../src/model/SpectralPresetModel.h
    title: Spectral preset contracts
  - resource: ../../src/model/SpectralPresetModel.cpp
    title: Spectral preset validation
  - resource: ../../src/model/ExpressionProgram.h
    title: Expression program contracts
  - resource: ../../src/model/ExpressionProgram.cpp
    title: Expression compiler and evaluator
---

# Purpose

Issue #6 introduces a JUCE-independent model boundary for editable source
spectra and ordered transfer layers. Untrusted, dynamically sized draft data is
validated into an owned, fixed-capacity representation before later DSP,
publication, persistence, or editor work can consume it. This contract does not
change the current fixed-harmonic audio path or expose a user workflow.

# Source definitions

`SourceDefinition` has four draft alternatives in stable order. A direct
partial declares one expanded partial; each generator declares its `count`.

| Draft alternative | Contract | Validated alternative |
| --- | --- | --- |
| `TrackedPartial` | Frequency ratio plus source level | `TrackedPartial`, copied unchanged |
| `FixedPartial` | Frequency in hertz plus source level | `FixedPartial`, copied unchanged |
| `HarmonicGenerator` | Count, base level, roll-off, even-harmonic offset, and inharmonic stretch | `HarmonicGenerator`, copied unchanged |
| `FormulaGenerator` | Frequency mode, count, frequency expression, and level expression | `ValidatedFormulaGenerator`, owning both compiled programs |

Formula frequency mode is either `trackedRatio` or `fixedHz`. Source formula
`index` values are one-based. The cumulative declared expansion across all
source definitions must not exceed 256 partials per note.

# Transfer definitions

`TransferDefinition` has five draft alternatives in stable order.

| Draft alternative | Contract | Validated alternative |
| --- | --- | --- |
| `FreeCurve` | One or more frequency/response points | `ValidatedFreeCurve`, owning fixed point storage |
| `PeakNotch` | Frequency, signed peak/notch gain, and Q | `PeakNotch`, copied unchanged |
| `Shelf` | Low/high kind, frequency, signed gain, and Q | `Shelf`, copied unchanged |
| `Tilt` | Pivot frequency and slope | `Tilt`, copied unchanged |
| `TransferFormula` | Frequency-response expression | `ValidatedTransferFormula`, owning the compiled program |

Each `TransferLayer` retains its `enabled` flag. Vector position is the sole
canonical transfer order; there is no second ordering field. Disabled layers
remain in that order and their complete payloads are validated, so enabling a
layer cannot reveal unchecked data.

# Capacities and numeric contracts

All bounds are inclusive. Every numeric value must be finite before its range
is checked.

| Item | Allowed range or capacity |
| --- | --- |
| Source definitions | 0 through 64 per preset |
| Expanded source partials | 0 through 256 per note |
| Transfer layers | 0 through 32 per preset |
| Free-curve points | 1 through 64 per curve, with strictly increasing unique frequencies |
| Harmonic or formula generator count | 1 through 256 |
| Tracked ratio | `[1e-6, 65536]` |
| Cents accepted by `centsToRatio` | `[1200 * log2(1e-6), 1200 * log2(65536)]` |
| Model or fixed-partial frequency | `[1, 192000]` Hz |
| Source level or harmonic base level | `[-96, 0]` dB |
| Harmonic roll-off | `[-48, +12]` dB/octave |
| Even-harmonic offset | `[-96, +24]` dB |
| Inharmonic stretch `B` | `[0, 0.01]` |
| Free-curve or transfer-formula response | `[-96, +24]` dB |
| Peak/notch or shelf gain | `[-24, +24]` dB |
| Peak/notch or shelf Q | `[0.1, 20]` |
| Tilt slope | `[-24, +24]` dB/octave |

Peak/notch, shelf, tilt, and free-curve frequencies use the model-frequency
range. A preset may contain no sources and no transfer layers; an empty free
curve is invalid.

# Checked musical helpers

Ratio and cents conversion use these exact equations:

```text
cents = 1200 * log2(ratio)
ratio = exp2(cents / 1200)
```

The ratio input or converted result must remain in `[1e-6, 65536]`.

For one-based harmonic index `i`, the stretched ratio is:

```text
ratio(i) = i * sqrt(1 + B * i * i)
```

Generated harmonic level is:

```text
level(i) = baseLevelDb + rollOffDbPerOctave * log2(i)
           + (i is even ? evenOffsetDb : 0)
```

Only this generated harmonic level is clamped after evaluation, to
`[-96, 0]` dB. Formula results outside their required ranges are rejected and
are never clamped.

# Restricted expressions

Expressions contain finite numeric literals, parentheses, constants `pi` and
`e`, the selected context's variables, and the following operators and
functions. Precedence runs from lowest to highest:

1. logical OR: `||`
2. logical AND: `&&`
3. equality: `==`, `!=`
4. comparison: `<`, `<=`, `>`, `>=`
5. additive: `+`, `-`
6. multiplicative: `*`, `/`
7. unary: `+`, `-`, `!`
8. primary literals, constants, variables, function calls, and grouped expressions

The exact function set is `if(condition, when_true, when_false)`, `abs(x)`,
`min(a, b)`, `max(a, b)`, `clamp(x, minimum, maximum)`, `pow(base, exponent)`,
`exp(x)`, `log(x)`, `sin(x)`, and `cos(x)`. Function arity is fixed. Assignment,
ternary syntax, loops, mutable state, implicit variables, and user-defined
functions are unsupported.

| Context | Available variables |
| --- | --- |
| Source frequency and level formulas | `index`, `fundamental_hz`, `nyquist_hz` |
| Transfer formulas | `frequency_hz`, `nyquist_hz` |

Unknown and cross-context identifiers are compile errors. Each expression is
limited to 512 characters, 128 parsed nodes, and 128 explicitly nested grouped
parentheses. Compilation produces a fixed owned program with a stable error
code, zero-based source offset, and static message on failure.

Evaluation is deterministic, `noexcept`, bounded by the parsed node count, and
performs no dynamic allocation. `if`, `&&`, and `||` are lazy, so an unselected
branch is not evaluated. Zero is false, nonzero is true, and comparison,
logical, and logical-not results normalize to `0.0` or `1.0`.

Compile failures distinguish empty or overlong input, node or nesting limits,
missing expressions, trailing or unsupported syntax, unknown identifiers or
functions, and wrong function arity. Evaluation rejects an invalid program or
result range, exhausted work budget, division by either sign of zero, invalid
`log`, `pow`, or `clamp` domains, non-finite evaluated variables or
intermediate results, and a final result outside the caller's finite ordered
inclusive range.

Callers evaluate a source frequency program against `[1e-6, 65536]` in
tracked-ratio mode or `[1, 192000]` Hz in fixed-Hz mode, and a source level
program against `[-96, 0]` dB. Transfer programs use `[-96, +24]` dB. Preset
validation compiles formulas but cannot evaluate context-dependent results;
the later caller must supply the runtime context and required range.

# Draft-to-validated boundary

`SpectralPresetDraft` owns editable `std::vector` collections and expression
`std::string` values. `validatePreset` checks top-level capacities first, then
visits sources, layers, and curve points in their canonical vector order. It
validates enum values, counts, finite scalars, inclusive ranges, free-curve
ordering, total declared expansion, disabled payloads, and expression
compilation.

Validation is all-or-nothing. Any failure leaves the result without a
`ValidatedSpectralPreset`; valid siblings are not partially published. The
stable error identifies its code and field, the exact zero-based source,
layer, and point indices where applicable, and the underlying expression code
and offset for compile failures.

The successful model owns fixed arrays and explicit counts. Free curves own
their fixed point arrays, and formula alternatives own their compiled
`ExpressionProgram` values; no validated payload borrows draft strings or
vectors. After the draft has been built, validation stores directly into this
fixed representation without dynamic allocation. Read-only accessors return
`std::span` views, which do borrow the
validated object and must not outlive or be retained across destruction,
replacement, or movement of that owner.

The maximum validation result is large because it contains fixed storage for
every source, layer, curve point, and compiled expression. On Windows Debug,
avoid keeping multiple by-value results alive on the stack at once. The
validator's keyed-constructor/prvalue return constructs directly in the
caller's result slot; callers that need multiple long-lived results should use
sequential lifetimes or suitable preallocated owning storage.

# Later-ticket boundaries

- Issue #7 owns expansion and DSP rendering of validated source models.
- Issue #8 owns transfer-layer response calculation, summing, and the final
  `[-96, +24]` dB clamp.
- Issue #9 owns persistence and publication of validated presets.
- Issue #10 owns editor controls for spectral sources and transfer layers.
- Issue #11 owns end-to-end workflow integration.

Until those tickets land, the processor, APVTS state, editor, and current
fixed-harmonic synthesis behavior do not consume this model.

# Related concepts

See [Architecture](architecture.md), [Spectral signal flow](signal-flow.md),
[Parameters and state](parameters.md), and
[Analyzer and real-time constraints](analyzer-and-realtime.md).
