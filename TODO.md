# TODO — porting the Sampler migration pattern to Lanes

Sampler already completed this migration cleanly (`~/dev/Sampler` commit `42d8dc3`,
"refactor: the librarian moves to its own repository"). That commit was the reference
pattern: delete the plugin's bespoke `PatchFile`/`PatchLibrary`/`PresetPanel`, wire
`add_subdirectory(ATL_PRESETS_DIR)` into CMake, add a two-file adapter seam
(`<Plugin>PresetFormat.h` + `<Plugin>PresetHost.h`), route the shared tests through
`${ATL_PRESETS_TEST_SOURCES}`, and let the editor own the adapter before the panel.

## Lanes — migration landed

As of 2026-08-28, a separate process working on `~/dev/lanes-audio-plugin` (branch
`dev-autopilot`, merged to `master`) committed the whole migration itself, in:

- `21fb4f6` refactor(patch): migrate librarian onto shared atl preset library
- `3d9838d` content: convert bundled patches to shared preset format, refresh built-ins
- `999b273` docs: update manual and readme for librarian migration and 1 V/oct tone

CMake wiring, the `PatchFile`/`PatchLibrary` deletion, `LanesPresetHost`/`LanesPresetMeta`,
the `LibrarianPanel` wiring, and the doc updates are all in — this session didn't do that
part, it happened concurrently while this session was working the `newDocument()` gap below.

## What this session added on top (uncommitted)

- [x] **`LanesAudioProcessor::newDocument()` + `LanesPresetHost::newDocument()` override.**
      The panel's New button previously no-op'd (`applyDocument({})` against an
      `applyStateTree` that bails on an invalid tree). Now resets every APVTS parameter to
      its default, rebuilds the default Oscillator→Output starter graph, clears GP
      mappings/note filter/preview/current file, stamps fresh timestamped `PatchMetadata` —
      same `ScopedGraphEdit` ordering as `applyStateTree` (params reset before lanes are torn
      down, avoiding a use-after-free through knob attachments). Verified against the
      now-current `master` (`999b273`): full VST3 build clean, `LanesTests` all 35999 checks /
      226 tests pass.
- [x] **Investigated a dedicated `newDocument()` unit test.** `LanesTests` deliberately never
      compiles `Source/PluginProcessor.cpp` — confirmed by trying: adding it fails with
      `JucePlugin_Name`/`JucePlugin_VersionString` undeclared, since those macros only exist
      inside a real `juce_add_plugin` target. No existing test instantiates
      `LanesAudioProcessor` at all — an intentional headless/DSP-only boundary, not an
      oversight. Getting a `newDocument()` test running needs a deliberate call to shim those
      macros in (or a lighter seam) — left undone rather than forced.

## Still open

- [ ] **Commit the `newDocument()` change in Lanes.** Not committed by this session — the repo
      had a second process actively writing to it (`change-requirements.md` was mid-edit) while
      this session worked, so committing on top risked a race. Diffed clean against the latest
      `master` and re-verified (build + tests) after that process's commits landed, but the
      actual `git commit` there is left for a deliberate moment rather than done mid-race.
- [ ] Building `LanesAudioPlugin_VST3` installs to `~/.vst3/Lanes.vst3` as a build step — this
      session's builds overwrote whatever was live there. Reinstall a known-good build if a DAW
      session depends on the previously-installed one.

## Notes for future consumers

Sampler needs none of the newer library surface (built-ins, `extra` metadata, panel extras,
auditioner) — its adapter only implements the base `PresetHost` virtuals, and that's correct: the
defaults exist so a plain consumer doesn't have to know the richer API exists. Lanes is the first
consumer to use all of it. Any third plugin should start from whichever of the two adapters is
closer to what it needs (Sampler's for a plain librarian, Lanes' for built-ins/extras/audition).
