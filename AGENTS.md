# Spektrummer agent guidance

Before planning or changing Spektrummer behavior, read
[`docs/okf/index.md`](docs/okf/index.md) and the concepts relevant to the change.
Treat the source code and tests as authoritative when a concept is stale or
ambiguous.

When a change affects product behavior, architecture, parameters, MIDI,
real-time constraints, build steps, or user workflow:

- Update every affected OKF concept.
- Keep concept YAML frontmatter parseable and retain a non-empty `type` field.
- Keep `docs/okf/index.md` navigable and its `okf_version: "0.2"` declaration.
- Add a dated entry to `docs/okf/log.md`.
- Verify relative links and descriptions before publication.

Follow the shared workspace `AGENTS.md` for JUCE, build, test, and audio-thread
requirements. Do not commit `.tickets/`; it is local ticket working state.
