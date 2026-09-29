# Autotel presets librarian (`atl`) — coding agent entry point

Shared preset librarian for the Autotel JUCE plugins: file format (`PresetFile`), folder scanner
(`PresetLibrary`), and the librarian panel (`PresetPanel` + `PresetPanelStyle`/`Views`,
`PresetEditPopover`). `README.md` is the design and the API; `Presets librarian redesign/SPEC.md`
is the panel's visual spec. Read the README's three rules before changing behaviour.

## Build and verify

No build or runner of its own — consumers pull it in with `add_subdirectory` and compile
`${ATL_PRESETS_TEST_SOURCES}` into their test binary. Verify every change through mepSynth:

```bash
cd ../mepSynth-plugin && ./build.sh test
```

A change to a `PresetHost` virtual or to panel behaviour a consumer relies on should also be built
in the other consumers: `../Sampler`, `../lanes-audio-plugin`, `../messy system plugin`,
`../modal-refractor-plugin`. Do not commit in those repos from here unless a task says so.

## Important

- Use `<juce_module/juce_module.h>` includes, NOT `<JuceHeader.h>`; JUCE is `../JUCE`, added by the
  consumer, never by this repo.
- `atl_presets_files` links `juce_data_structures` only — no GUI in the file/scanner layer.
- The library knows nothing about what a document means (README rule 2). Plugin-specific needs go
  through `PresetHost` virtuals with sensible defaults, so a plain consumer never has to know them.
- Durable knowledge goes in `README.md`, not here or in `tasks.md` — never the same fact twice.

## Product development input pipeline

Defined by `working-logic.md`:
- New requirements go in `change-requirements.md`. **Commit that file on its own, as filed, before
  converting anything.** Opus then turns the bullets into detailed tasks in `tasks.md`, each
  tagged with the model that should implement it, and empties the requirements file. Implementers
  work from `tasks.md` and delete entries as they land.
- If `tasks.md` has open work: implement it step by step.
- If `tasks.md` is empty but `change-requirements.md` is not: Opus writes the plan first.
- If both are empty: hunt bugs, or make the code/docs/agent setup leaner.

## Unattended runs

`./continue-work.sh` runs this pipeline headlessly (`claude -p "continue the work"`, auto mode,
one run at a time via flock), logging to `~/.local/state/atl-continue/` with `latest.log`
symlinked to the newest. Schedule with
`echo /home/joaquin/dev/autotel-plugin-presets-librarian/continue-work.sh | at 00:02`.
It shares no lock with mepSynth's runner, so don't queue both at once — both build mepSynth.
