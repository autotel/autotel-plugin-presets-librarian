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

## Browsing is not loading (2026-08-31, uncommitted)

Selecting a row used to load it into the live document. It no longer does: selecting only fills the
details column, and a new **load** button — with double-click and Return as shortcuts — is the only
thing that loads. Rule 3 in the README. The reason is the one the auditioner already implied: a
plugin whose `applyDocument` rebuilds a graph or rereads sample files should pay for that when
asked, not on every arrow-key press, and browsing should never be able to lose unsaved work.

- Librarian: `PresetPanel::doLoad()`, the button, the two `ListBoxModel` shortcut hooks; the
  auto-load dropped from `selectionChanged()`. Loading the already-open row is allowed on purpose —
  that is revert-to-disk.
- Tests: five `atl::PresetPanel` cases — select-does-not-load, Load fires `documentLoaded` and not
  `documentSaved`, both shortcuts, and a shortcut aimed at a row that is not the selection.
- Verified in both consumers. Sampler: full build + suite clean. Lanes: `LanesAudioPlugin_VST3`
  clean, `LanesTests` 36008 checks / 227 tests, exit 0. No Lanes logic depended on the old
  behaviour — `LibrarianPanel::selectionChanged()` never loaded, and `onPatchLoaded` hangs off the
  host's load path either way.
- One visible Lanes behaviour change, for the better: arrow-keying the list no longer switches the
  editor to the Synthesis tab on every row, because that switch rode on the auto-load.
- Lanes doc/comment fixes are **uncommitted in `~/dev/lanes-audio-plugin`**: `LibrarianPanel.h`'s
  header comment, and `manual.md`'s rule 2 plus its button list (which was also missing **new**
  from an earlier round).

## Still open

- [ ] **Commit the `newDocument()` change in Lanes.** Not committed by this session — the repo
      had a second process actively writing to it (`change-requirements.md` was mid-edit) while
      this session worked, so committing on top risked a race. Diffed clean against the latest
      `master` and re-verified (build + tests) after that process's commits landed, but the
      actual `git commit` there is left for a deliberate moment rather than done mid-race.
- [ ] Building `LanesAudioPlugin_VST3` installs to `~/.vst3/Lanes.vst3` as a build step — this
      session's builds overwrote whatever was live there. Reinstall a known-good build if a DAW
      session depends on the previously-installed one.
- [ ] **Lanes bug found while documenting the load path: `notePatchOpened()` gets the wrong file.**
      `PresetPanel::loadEntry()` calls `host.applyDocument()` before `host.documentLoaded()`.
      `LanesPresetHost::applyDocument()` fires `onPatchLoaded` (LanesPresetHost.cpp:80), whose
      handler runs `autosave_.notePatchOpened(processor_.getCurrentPatchFile())`
      (LibrarianPanel.cpp:119) — but `setCurrentPatchFile(file)` does not happen until
      `documentLoaded()` (LanesPresetHost.cpp:127). So autosave is told about the *previously* open
      file, and about an empty file on the first load of a session. The callback's whole job is
      "autosave must not start writing over the patch just opened", so it is guarding the wrong one.
      Predates the browsing-is-not-loading change — the ordering was the same when selection loaded.
      Not fixed here because the fix is a design call: reorder the two host calls in the shared
      panel (which moves Sampler too), or fire `onPatchLoaded` from `documentLoaded` in Lanes and
      decide what `newDocument()` — which fires it with no file at all — should do.
- [ ] ~~**`developer-manual.md:1297-1311` in Lanes is stale, and predates this change.**~~ Done
      2026-08-31, uncommitted: rewrote **Index cache** → **Scanning and built-ins** (no cache exists
      any more; built-ins are a flat `BinaryData` conversion, not derived), rewrote **Librarian
      tab**, corrected **File format** (the shared `writeToStream`/`<PresetMeta>` format, the
      `<LanesMeta>` extra seam, and a pointer to `BREAKING-CHANGES.md` for the conversion), and
      dropped the deleted `PatchFile`/`PatchLibrary` entries from the source-tree diagram. Original
      note follows.

      **`developer-manual.md:1297-1311` in Lanes is stale, and predates this change.** Its "Index
      cache" and "Librarian tab" sections still describe the bespoke `PatchLibrary` and
      `processor_.loadStateFromFile()`; `Source/Patch/PatchLibrary.*` was deleted in the migration
      and `grep -rn "PatchLibrary" Source/` now returns nothing. Left alone rather than folded into
      the browsing-is-not-loading pass, because it is a different piece of rot.

## Panel redesign (2026-09-22)

Landed: the librarian tab was rebuilt around the preset instead of a details column — folder bar
with `folder…`/`migrate…`, filter bar, active tag filters, a tag rail with counts, the pinned
live-patch strip (dirty chip, `edit…`/`save`/`revert to disk`), rows with a star/tags/comment and a
selected-row action strip, the row-anchored edit popover, and a status bar with `new`/`save as`.
Loading now passes a dirty guard (`save, then load` / `load anyway` / `cancel`, policy in
`PresetPanelOptions::loadGuard`) instead of loading straight over unsaved work. See `README.md` and
`Presets librarian redesign/SPEC.md` for the shape of it.

- [ ] **Consumers should implement `PresetHost::documentHasUnsavedChanges()`.** It defaults to
      `true`, so a host that hasn't implemented it gets the load-over-dirty prompt on every single
      load. Sampler, Lanes, mepSynth, `messy system` and `modal refractor` all need a real answer
      wired to their own dirty tracking.
- [ ] **Lanes' `detailsHeader` waveform preview now only shows inside the edit popover**, not beside
      the fields all the time — the details column it used to sit in is gone. Decide whether Lanes
      wants the waveform somewhere else too, or whether popover-only is fine.
- [ ] **Favourites persistence is up to each plugin.** The panel keeps favourites in memory
      (`getFavourites`/`setFavourites`/`onFavouritesChanged`) but does nothing to make them outlast
      the editor; every consumer that wants them to survive a restart has to persist and restore the
      list itself.
- [ ] **A host that remembers the open preset's path should override `migratePresetFolder`.** The
      panel follows a moved file for its own bookkeeping, but no `PresetHost` virtual tells the
      plugin the file moved, so a plugin-side path (an autosave target, a "recently opened" entry)
      goes stale after a migration unless the override follows it.
- [ ] **messy system plugin's `Tests/main.cpp` has no `juce::ScopedJuceInitialiser_GUI`.** Its
      tests pass, but the librarian's panel tests now build real components, so the process ends
      with a long tail of leaked-object reports and leak-detector assertions at static teardown.
      mepSynth, Sampler and Lanes already construct one in their test mains; Messy should too.
      (Verified 2026-09-22: all five consumers build and pass their suites against this redesign.)

## Notes for future consumers

Sampler needs none of the newer library surface (built-ins, `extra` metadata, panel extras,
auditioner) — its adapter only implements the base `PresetHost` virtuals, and that's correct: the
defaults exist so a plain consumer doesn't have to know the richer API exists. Lanes is the first
consumer to use all of it. Any third plugin should start from whichever of the two adapters is
closer to what it needs (Sampler's for a plain librarian, Lanes' for built-ins/extras/audition).
