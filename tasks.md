# Tasks

Open work only. Each entry carries the model that should implement it -- `[opus]` for the
delicate, the architectural or the not-obviously-right; `[sonnet]` for the mechanical or the
fully specified. Delete an entry as it lands. Durable knowledge goes in `README.md`, never here.

## Consumer follow-ups

Checked 2026-09-29 against each consumer's `Source/`. mepSynth, Lanes and messy system are done.

- [sonnet] **Sampler and modal refractor don't implement `PresetHost::documentHasUnsavedChanges()`.**
  It defaults to `true`, so every load there prompts the dirty guard. Wire it to each plugin's own
  dirty tracking (add one if there is none). Work happens in those repos, not this one.
- [sonnet] **Consumers other than mepSynth forget the open preset when their window closes.**
  `PresetHost::openDocumentKey()` / `openDocumentKeyChanged()` (see README) default to keeping
  nothing. messy system already keeps `currentPatchFile_` on its processor -- answer with it (and
  take the key, since a built-in has no file). Sampler, Lanes, modal refractor: keep the key on the
  processor and in its state. Work happens in those repos.
- *Not a task yet:* Lanes' `detailsHeader` waveform preview only shows inside the edit popover
  since the redesign removed the details column. Needs the author's word on whether Lanes wants it
  elsewhere.
