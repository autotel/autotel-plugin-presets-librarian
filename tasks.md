# Tasks

Open work only. Each entry carries the model that should implement it -- `[opus]` for the
delicate, the architectural or the not-obviously-right; `[sonnet]` for the mechanical or the
fully specified. Delete an entry as it lands. Durable knowledge goes in `README.md`, never here.

## Round 1 -- three filed bullets (456643c)

- [opus] **A new patch's author defaults to the last author who saved.** Filed as: "Whenever
  saving new patches, author name should be auto-filled with the last saved patches' author
  name." Today `PresetPanel::liveMetadata()` (PresetPanel.cpp ~790) falls back to the live
  document's `author` property and nothing else, so a patch that has never been saved starts with
  an empty author. Take the fallback from the library rather than adding persistence: the
  user-folder row (not a built-in) with the newest `meta.date` and a non-empty author is "the last
  saved patch". Apply it only where the author is still empty after the document was asked -- a
  patch's own author always wins, and an existing file is never rewritten with someone else's
  name. Covers both the save/`save as` path and the edit popover opened on an unsaved live patch,
  which should show the prefilled name so it can be corrected before saving. Test in
  `PresetPanelTests.cpp`: save A with author "x", start `new`, save B with no author -> B's file
  carries "x"; a patch whose document already names an author keeps it.
- [sonnet] **Tags ignore case everywhere, not just in the picker.** Filed as: "Casing should be
  ignored when typing tags." `TagPicker::addTag` already folds a typed tag onto an existing
  spelling, but `PresetLibrary` treats tags case-sensitively: `allTags()` / `tagCounts()`
  (PresetLibrary.cpp ~71-96) list "Bass" and "bass" as two tags, and the tag filter (~191-197)
  and `PresetPanel::assignTag` (~751) use case-sensitive `contains`. Make all of them
  case-insensitive: one tag per case-folded spelling, counted together, displayed with the
  spelling that appears first in scan order (or the most common -- pick one and say which in the
  README's Tags section); filtering on "bass" matches a row tagged "Bass"; dropping "bass" on a
  row tagged "Bass" is "already tagged". Also check `TagPicker::rebuild()`'s
  `tags.contains (t, true)` stays consistent. Tests in `PresetLibraryTests.cpp` for counts and
  filtering across mixed case.
- [sonnet] **A comma in the tag input commits the tag.** Filed as: "when typing a comma into the
  tags input, may the typed tags be separated by commas". Today `addTag` strips commas and only
  Return commits. In `TagPicker`'s `input.onTextChange` (PresetEditPopover.cpp ~34): when the text
  contains a comma, split on commas, `addTag` every complete piece, and leave the text after the
  last comma in the input. Pasting "a, b, c" therefore adds a and b and leaves "c" typed; Return
  commits the rest as before. Update the placeholder hint if it helps ("type, comma or return to
  add…"). Test in `PresetPanelTests.cpp` via the popover's tag picker.

## Consumer follow-ups

Checked 2026-09-29 against each consumer's `Source/`. mepSynth, Lanes and messy system are done.

- [sonnet] **Sampler and modal refractor don't implement `PresetHost::documentHasUnsavedChanges()`.**
  It defaults to `true`, so every load there prompts the dirty guard. Wire it to each plugin's own
  dirty tracking (add one if there is none). Work happens in those repos, not this one.
- *Not a task yet:* Lanes' `detailsHeader` waveform preview only shows inside the edit popover
  since the redesign removed the details column. Needs the author's word on whether Lanes wants it
  elsewhere.
