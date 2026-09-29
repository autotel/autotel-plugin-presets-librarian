# Presets librarian — redesign spec

Design file: `Presets Librarian.dc.html` (interactive, opens in a browser).
Source it describes: `autotel/autotel-plugin-presets-librarian` (`atl::PresetPanel`, `atl::PresetLibrary`, `atl::PresetFile`).

The redesign changes the panel's *shape* and three *behaviours*. It does not change the file format, the
scanner, or the `PresetHost` contract. Two host virtuals are new (§8).

---

## 1. Why it changed

Three problems in the old panel:

1. **Split identity.** The list on the left and the editable fields on the right described the same preset
   in two places, with nothing tying them together. Which row the fields belonged to was inferred, not shown.
2. **Unsafe load.** Double-click loaded immediately. Loading replaces the live patch, so a mis-aimed
   double-click could destroy an hour of work with no warning and no way back.
3. **Tags were a text field and a combo box.** Assigning a tag meant typing a comma-separated string;
   filtering meant picking one tag from a dropdown. Nothing showed which tags existed or how many presets
   carried them.

The redesign answers each: fields move *to* the preset, loading passes a guard, and tags become
first-class objects in the UI.

---

## 2. Layout

Single column stack, minimum usable size **900 × 600**. Everything is fluid above that.

```
┌──────────────────────────────────────────────────────────────────────────────┐
│ PRESETS   /home/…/MEP_SYNTH/Presets            [ folder… ] [ migrate… ]     │  folder bar
├──────────────────────────────────────────────────────────────────────────────┤
│ [ filter name, author, comment…      ] [built-in|mine] [ sort ▾ ↑A–Z] [⟳]   │  filter bar
├──────────────────────────────────────────────────────────────────────────────┤
│ filtering by  ( pad × )( bell × )  [match all]  clear                       │  active filters
├───────────────┬──────────────────────────────────────────────────────────────┤
│ TAGS          │ LIVE PATCH  Reso synth  joaquin · 2026-09-21                │  pinned strip
│ bass       1  │             ● unsaved    [edit…] [save] [revert to disk]    │
│ bell       2  ├──────────────────────────────────────────────────────────────┤
│ bright     1  │ ☆ Vinyl Bed — resonant  FACTORY  (pad)(texture)  Factory ·…  │
│ init       2  │   wide, slow filter drift                                    │  list
│ lead       1  │ ★ glocken  (bell)                          joaquin · 09-21   │
│ pad        3  │   the newer one — keep                                       │
│ texture    3  │   [ load ] [ edit… ] [ duplicate ] [ delete ]                │  ← selected row
│               │   ┌────────────── edit popover ──────────────┐               │
│ drag a preset │   │ file name / author / comment / …          │               │
│ onto a tag to │   └──────────────────────────────────────────┘               │
│ assign it     │                                                              │
├───────────────┴──────────────────────────────────────────────────────────────┤
│ loaded “Glass Bell”                                    [ new ] [ save as ]  │  status bar
└──────────────────────────────────────────────────────────────────────────────┘
```

**Neutrality.** The palette is `atl::PresetPanelColours`' own defaults — `background #1a1a1c`,
`panel #232327`, `text #e4e4e8`, `textDim` lifted to `#9b9ba3` for contrast, `accent #4aa3ff`,
`outline #3a3a40` — so the panel reads as chrome inside any plugin skin, and `setColours` keeps working
as the per-plugin override. Type is one neutral grotesque (IBM Plex Sans) with a mono
(IBM Plex Mono) reserved for machine text: paths, dates, counts, the extension. No decoration,
no gradients, 3 px radii, one accent used only for *the live patch* and *the current filter*.

---

## 3. The pinned live patch

A sticky strip at the top of the list, accent-barred, above every row and unaffected by sort or filter.
It answers "what am I about to lose" without the user having to find a row.

| Element | Content |
| --- | --- |
| Label | `LIVE PATCH` |
| Name | open preset's name, or `new patch — never saved` |
| Meta | `author · date` of the open preset (hidden when nothing is open) |
| State chip | `● unsaved changes` (amber) / `○ saved` (dim) |
| `edit…` | opens the edit popover directly under the strip (§5) |
| `save` | accent when there is a file *and* unsaved edits; disabled otherwise |
| `revert to disk` | re-reads the open preset; enabled only when dirty |

The open preset also stays visible in the list, marked with an accent left bar and an `OPEN` badge —
the strip is a pin, not a move.

*In the demo the state chip is clickable so the dirty paths can be exercised. In the plugin it is a
read-out, driven by the host.*

---

## 4. Loading, and the dirty guard

Rule 3 of the library ("browsing is not loading") stays: selection never touches the live document.
Load is still the only thing that loads, still reachable from the row's `load` button, a double-click,
and Return.

What changes is what Load does when there is something to lose:

- **Live patch clean** → loads immediately, no dialog. Browsing and loading stay fast.
- **Live patch dirty** → one modal:
  > **Load "Glass Bell"?**
  > "Reso synth" has unsaved changes. Loading replaces the live patch and those edits are gone.
  > `[ save, then load ]` `[ load anyway ]`  ·  `cancel`
- **Loading the already-open preset** → that is revert-to-disk, and is allowed; it prompts only if dirty.

`save, then load` is the default-looking action because it is the one that loses nothing. `load anyway`
is a plain button, `cancel` is text. A clean patch never sees this dialog, so the guard costs nothing
in the common case and cannot be trained away by repetition.

Policy is one knob (`loadGuard`): `dirty` (default) · `always` · `none`.

*Considered and rejected for now: an A/B slot, and a rolling auto-snapshot history. Both are better
recovery than a prompt, but both need the plugin to hold a second document — real work in the host,
not in the librarian. The guard is the part that belongs to the panel.*

---

## 5. Editing metadata — the row-anchored popover

The right-hand column is gone. The fields open *at* the preset they describe.

- Single click **selects** a row: it highlights and grows an action strip — `load`, `edit…`,
  `duplicate`, `delete` — plus a one-line hint (`double-click a row to load it`, or for a factory
  preset, `edit writes a copy into your folder`).
- `edit…` opens a popover attached under that row (or under the pinned strip, for the live patch).
  Only one is open at a time.

Popover contents, in a two-column grid:

| Field | Notes |
| --- | --- |
| **file name** | with `.mepsynth` shown dimmed beside it. The name *is* the filename; applying a changed name renames the file. |
| **author** | |
| **comment** | labelled *shown in the list, never part of the name* |
| **description** | two-row textarea |
| **tags** | chips with `×`, plus a `type to add or create…` input; existing tags appear underneath as dashed suggestions, filtered as you type; Return commits, creating the tag if it is new |

Footer: `apply` (accent) · `cancel` · a hint that states the consequence —
`a name change renames the file on disk`, or for a factory preset,
`writes an editable copy into your folder`.

`apply` on a factory preset produces a user copy rather than an error, matching
`PresetLibrary::renamePreset`'s refusal-plus-duplicate rule.

---

## 6. Tags

Four surfaces, one model.

1. **Tag rail** (left, 176 px, hideable): every tag in the library with a count. Click toggles it as a
   filter; active tags get an accent bar. The rail is the answer to "what tags do I even have".
2. **Row chips**: each preset shows its tags; clicking one toggles that tag as a filter, so filtering
   is available from wherever you noticed the tag.
3. **Drag to assign**: drag a preset onto a rail tag. The rail item highlights as a drop target, the
   dragged row dims, and the status bar confirms (`tagged "glocken" as bell`, or
   `"glocken" already tagged bell`). This is the fast path for tagging a batch of presets.
4. **Tag picker** in the popover (§5): the precise path — add, remove, create.

**Active filter bar** appears only when tags are selected: each tag as a removable chip, a
`match all` / `match any` toggle (AND/OR), and `clear`. Text filter and source toggles combine with it,
matching `PresetFilter` (text over name/author/description/comment, `requiredTags`, source flags,
then the plugin's `extraPredicate`).

---

## 7. Sorting

A select plus a direction toggle in the filter bar: **name · date modified · author · first tag ·
favourites first · factory/mine**. Switching to *date* or *favourites* flips direction to descending,
since that is what those sorts are for. Secondary key is always name, so order is stable. The pinned
live patch is outside the sort.

Favourites are a per-preset star on the left of each row — a click, no dialog. (Persistence: in
`PresetMetadata::extra`, or a panel-local list, at the plugin's discretion.)

---

## 8. Folder and migrate

The folder bar shows the current preset folder as mono text, with two buttons:

- **folder…** — the existing behaviour: point the library at a different folder. Nothing is moved.
  Status says so: `preset folder changed — nothing was moved`.
- **migrate…** — new. A two-step modal:

  **Step 1 — Migrate preset folder**
  `from` (current path, read-only) · `to` (path field + `browse…`) · a summary line:
  *"N user presets will be moved. Factory presets stay in the plugin. The library then points at the
  new folder."* `move presets` stays disabled until a destination is chosen and differs from the source.

  **Step 2 — Moved N presets**
  *"The library now points at the new folder. Delete the old one?"* with the old path shown, and
  `keep it` (default-looking) / `delete old folder` (destructive styling).

  Deleting is never implied by migrating, and the destructive option is never the emphasised one.

Implementation notes for the real thing: move files individually, skip anything that is not a preset
file, and if the destination already holds a file of that name resolve it with
`PresetFile::resolveCollision` rather than overwriting. On any failure, stop, keep the source folder as
the live one, and report which files moved. Step 2 is offered only if the source folder is empty of
non-preset content — otherwise it says so and offers `keep it` only. The old folder goes to the system
trash, never a hard delete.

---

## 9. Status bar

Left: mono status text — the last thing that happened (`loaded "Glass Bell"`,
`renamed to "glocken 2" and wrote metadata`, `moved 8 presets`, `deleted — moved to trash`,
`factory presets cannot be deleted`). Right: `new`, `save as`. `save` lives on the pinned strip,
because it acts on the live patch and belongs next to its dirty state.

---

## 10. Destructive actions

| Action | Confirmation |
| --- | --- |
| Load over a dirty patch | modal, §4 |
| Delete a preset | modal — *Delete "x"? Moved to the system trash, not erased.* (`confirmDelete` knob) |
| Delete old folder after migrate | modal step 2, §8 — opt-in, destructive styling |
| Rename via apply | none — it is reversible by renaming back |
| Duplicate, star, tag | none |

Factory presets: `delete` is disabled, `edit… → apply` writes a copy, `duplicate` is the deliberate
route to an editable version, and the copy takes the built-in's own name
(`PresetLibrary::duplicatePreset`).

---

## 11. Host-side additions

Everything above works with the existing `PresetHost` except two things:

```cpp
/** Whether the live document has edits not yet written to its file. Drives the dirty guard
    and the pinned strip's state chip. A host that cannot tell should answer true. */
virtual bool documentHasUnsavedChanges() const { return true; }

/** Move every preset file from the current folder to `dest`, then adopt `dest`. Returns what
    moved and what was skipped so the panel can report it. */
virtual MigrationResult migratePresetFolder (const juce::File& dest) { … }
```

`documentHasUnsavedChanges()` defaulting to `true` means a host that does not implement it gets the
prompt every time — annoying but never destructive, which is the right default. The panel does not
attempt to infer dirtiness by comparing trees.

---

## 12. Knobs in the design file

| Prop | Default | Effect |
| --- | --- | --- |
| `showTagRail` | `true` | show/hide the left tag rail |
| `showComments` | `true` | show the comment snippet under each row |
| `loadGuard` | `dirty` | `dirty` · `always` · `none` |
| `confirmDelete` | `true` | confirm before deleting a preset |

---

## 13. Not in this pass

- Auditioning (`PresetAuditioner`) — the button belongs next to `load` on the selected row; unchanged in
  behaviour, so it was left out of the mock rather than redesigned.
- `PresetPanelExtras` slots (waveform above the fields, plugin-specific filters under the search row).
  Both still have a place: the extras header goes inside the popover above `file name`; extra filters
  go in the filter bar.
- Keyboard model beyond Return-to-load: arrow keys move the selection, `/` should focus the filter,
  `F2` should open the popover on the selection, `Escape` closes it.
- Multi-select, and therefore batch tagging/deleting. Drag-to-assign is the current batch path and it
  is one preset at a time.
  
  
# Ammend to the spec
One thing to add to the spec:
in the current html demonstration, the "new, unsaved patch" is impossible to save because save and edits are disabled. They should be active and enabled, so that the user can, indeed, change the title of the live patch and save it.
