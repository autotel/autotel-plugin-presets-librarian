# Tasks

Open work only. Each entry carries the model that should implement it -- `[opus]` for the
delicate, the architectural or the not-obviously-right; `[sonnet]` for the mechanical or the
fully specified. Delete an entry as it lands. Durable knowledge goes in `README.md`, never here.

## Panel

- [opus] **One-click load column on every row.** Loading presets one after another today costs
  select-then-`load`, or a double-click. Give every row a `load` chip in a fixed column at the
  row's right edge (right of the meta), painted like the rest of the row: faint outline at rest,
  accent on hover. One click on it selects that row and fires `rowLoadRequested` -- the same path as
  `load`/Return/double-click, so the dirty guard still applies (rule 3 holds: it is an explicit
  Load, not browsing). It must not start a drag, and the second click of a double-click on it must
  not load again. Expose the chip's area for tests (`PresetListView::loadAreaForRow`), add a panel
  test (click it on an unselected row: loads, selects it, never saves), and update README rule 3 and
  the tests paragraph.

## Consumer follow-ups

Checked 2026-09-29 against each consumer's `Source/`. mepSynth, Lanes and messy system are done.

- [sonnet] **Sampler and modal refractor don't implement `PresetHost::documentHasUnsavedChanges()`.**
  It defaults to `true`, so every load there prompts the dirty guard. Wire it to each plugin's own
  dirty tracking (add one if there is none). Work happens in those repos, not this one.
- *Not a task yet:* Lanes' `detailsHeader` waveform preview only shows inside the edit popover
  since the redesign removed the details column. Needs the author's word on whether Lanes wants it
  elsewhere.
