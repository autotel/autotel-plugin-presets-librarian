# Tasks

Open work only. Each entry carries the model that should implement it -- `[opus]` for the
delicate, the architectural or the not-obviously-right; `[sonnet]` for the mechanical or the
fully specified. Delete an entry as it lands. Durable knowledge goes in `README.md`, never here.

## Consumer follow-ups

Checked 2026-09-29 against each consumer's `Source/`. mepSynth, Lanes and messy system are done.

- [sonnet] **Sampler and modal refractor don't implement `PresetHost::documentHasUnsavedChanges()`.**
  It defaults to `true`, so every load there prompts the dirty guard. Wire it to each plugin's own
  dirty tracking (add one if there is none). Work happens in those repos, not this one.
- *Not a task yet:* Lanes' `detailsHeader` waveform preview only shows inside the edit popover
  since the redesign removed the details column. Needs the author's word on whether Lanes wants it
  elsewhere.
