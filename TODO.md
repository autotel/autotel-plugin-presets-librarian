# TODO — porting the Sampler migration pattern to Lanes

Sampler already completed this migration cleanly (`~/dev/Sampler` commit `42d8dc3`,
"refactor: the librarian moves to its own repository"). That commit is the reference
pattern: delete the plugin's bespoke `PatchFile`/`PatchLibrary`/`PresetPanel`, wire
`add_subdirectory(ATL_PRESETS_DIR)` into CMake, add a two-file adapter seam
(`<Plugin>PresetFormat.h` + `<Plugin>PresetHost.h`), route the shared tests through
`${ATL_PRESETS_TEST_SOURCES}`, and let the editor own the adapter before the panel.

Lanes (`~/dev/lanes-audio-plugin`) is mid-way through the same migration. Status as of
2026-08-28:

## Already done in Lanes (uncommitted)

- [x] `CMakeLists.txt` wired: `ATL_PRESETS_DIR`, `add_subdirectory`, links `atl_presets_ui`,
      `LanesTests` pulls in `${ATL_PRESETS_TEST_SOURCES}`.
- [x] `Source/Patch/PatchFile.{h,cpp}` and `PatchLibrary.{h,cpp}` staged for deletion.
- [x] `Source/Patch/LanesPresetHost.{h,cpp}` and `LanesPresetMeta.{h,cpp}` added (untracked),
      implementing `atl::PresetHost` + `atl::PresetAuditioner`.
- [x] `Source/UI/LibrarianPanel.cpp` wires `setExtras`, `setBuiltIns`, `setAuditioner`.
- [x] Full VST3 build and `LanesTests` build both succeed against librarian commit `63d9fe3`.

## Remaining

- [x] **`LanesPresetHost::newDocument()` override.** Added. `LanesAudioProcessor::newDocument()`
      (`Source/PluginProcessor.cpp`, next to `applyStateTree`) resets every APVTS parameter to its
      default, rebuilds the default Oscillator→Output starter graph via `LaneGraph::fromValueTree({})`
      + `setupDefaultPatch()`, clears GP mappings/note filter/patch preview/current file, and stamps a
      fresh timestamped `PatchMetadata` — same ordering and thread-safety guard (`ScopedGraphEdit`) as
      `applyStateTree`. `LanesPresetHost::newDocument()` calls it and fires `onPatchLoaded`. Full VST3
      build and `LanesTests` (all 35425 checks, 226 tests) pass against librarian commit `63d9fe3`.
      Not covered by a dedicated new test yet — worth adding one exercising New end-to-end.
- [ ] **Split the working tree into separate commits before committing.** Lanes' git status
      currently mixes three unrelated changesets: this librarian migration (now including the
      `newDocument()` fix above), the 1 V/octave convention change (see `BREAKING-CHANGES.md`,
      `Source/DSP/LaneTypes.h`, `Examples/**/*.lanes`), and whatever else is sitting dirty. Commit
      them separately, the way Sampler's migration landed as one focused commit.
- [ ] **Commit the migration itself**, now that `newDocument()` is fixed — same shape as `42d8dc3`:
      what moved, what the two seam files are, why the editor owns the adapter before the panel.
- [ ] Add a unit test for `LanesAudioProcessor::newDocument()` / the panel's New button, since the
      existing suite doesn't exercise it yet.
- [ ] Update Lanes' own docs (`readme.md`, `manual.md`, `to-dos.md`) to point at the shared
      librarian, the way Sampler's `wiki/patches.md` was updated in `42d8dc3`.
- [ ] Note: building `LanesAudioPlugin_VST3` installs to `~/.vst3/Lanes.vst3` as a build step,
      overwriting whatever build was live there — including this session's WIP mix. Reinstall a
      known-good build if a DAW session depends on the previously-installed one.

## Notes for future consumers

Sampler needs none of the newer library surface (built-ins, `extra` metadata, panel extras,
auditioner) — its adapter only implements the base `PresetHost` virtuals, and that's correct: the
defaults exist so a plain consumer doesn't have to know the richer API exists. Lanes is the first
consumer to use all of it. Any third plugin should start from whichever of the two adapters is
closer to what it needs (Sampler's for a plain librarian, Lanes' for built-ins/extras/audition).
