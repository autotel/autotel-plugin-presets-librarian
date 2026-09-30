# Autotel plugin presets librarian

The preset/patch librarian shared by the Autotel plugins: one file format, one folder scanner, one
panel, so saving and browsing presets works the same way in every one of them.

## What it is

- `atl::PresetFile` — reads and writes a preset file: a `juce::ValueTree` document with a
  `<PresetMeta>` child appended, written with `ValueTree::writeToStream`. Also reads one straight
  out of memory, for presets a plugin ships inside its own binary.
- `atl::PresetLibrary` — scans a folder into rows, filters them, and does delete / rename /
  duplicate. Built-in presets list alongside the folder's.
- `atl::PresetPanel` — the librarian tab. A folder bar (`folder…` to point the library elsewhere,
  `migrate…` to move the files there) sits over a filter bar (text, source, sort) and, while any
  tags are filtering, a row of active-tag chips. Below that: a tag rail with a count per tag, and
  the list — the pinned live-patch strip on top (its dirty chip, `edit…`, `save`, `revert to disk`),
  then rows with a star, tag chips, a comment and a one-click `load` chip, each one growing an action strip
  (`load` / `edit…` / `duplicate` / `delete`) when selected. `edit…`, on a row or the strip, opens
  a popover anchored under it. A status bar carries the last thing that happened, plus `new` and
  `save as`. The panel's own drawing lives in `PresetPanelStyle` (colours, fonts, look-and-feel),
  its components in `PresetPanelViews` (tag rail, active-filter bar, live strip, list, modal), and
  the metadata fields in `PresetEditPopover`.

Three rules the rest of the design follows from:

1. **A preset's name is its filename.** `save()` stamps the file's base name into the document it
   writes; `load()` reads the name back off the file. Renaming a file in a file manager renames the
   preset, and there is never a second name to reconcile. The annotation a filename cannot carry
   goes in `comment`, which is shown in the list and is never part of a name.
2. **The librarian does not know what a document means.** It moves `juce::ValueTree`s around. What
   a plugin's tree contains, and what happens when one is loaded, is the plugin's business.
3. **Browsing is not loading.** Selecting a row highlights it and grows its action strip; it never
   touches the live document. **Load** is the only thing that does — double-clicking a row,
   pressing Return, and the `load` chip at the right edge of every row (one click, selected or not,
   for going through presets one after another) are shortcuts for it. Scrolling the list can't lose unsaved work, so a plugin
   whose `applyDocument` is expensive or disruptive — one that rebuilds a graph, rereads sample
   files, interrupts audio — pays for it only when asked. Hearing a row without loading it is what
   `PresetAuditioner` is for.

   Loading is still not allowed to lose work silently. When the live patch is dirty, Load stops for
   one modal — *Load "x"? "y" has unsaved changes. Loading replaces the live patch and those edits
   are gone.* — with `save, then load` (the default-looking action, because it loses nothing),
   `load anyway`, and `cancel`. A clean live patch loads immediately, no dialog, so the guard costs
   nothing in the common case. Loading the already-open preset is revert-to-disk, and is allowed; it
   only prompts if dirty. The policy is one knob, `PresetPanelOptions::loadGuard`: `dirty` (the
   default — asks only when the host reports unsaved changes), `always` (asks every time something
   would be replaced, except reverting a clean open preset), or `none`. `new` throws the live patch
   away exactly as surely as a load does, so it passes the same guard.

A patch getting its first file — a new one, one loaded from a built-in, or a `save as` — is signed
with the author of the most recently saved preset in the folder when neither the patch nor its
document names one; the edit popover shows that name before the save, so it can be changed. There
is no setting to keep: the folder already remembers who saved last. Saving over an existing file
never re-signs it.

## Using it from a plugin

```cmake
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../JUCE JUCE_build)   # JUCE first
set(ATL_PRESETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../autotel-plugin-presets-librarian"
    CACHE PATH "Where the shared presets librarian lives")
add_subdirectory(${ATL_PRESETS_DIR} atl_presets_build)

target_link_libraries(MyPlugin PRIVATE atl_presets_ui)
target_sources(MyPluginTests PRIVATE ${ATL_PRESETS_TEST_SOURCES})   # optional, but do it
```

Then describe the plugin's own files and implement one adapter:

```cpp
atl::PresetFormat format { ".myplugin", juce::Identifier ("MyDocument") };

struct MyHost : atl::PresetHost
{
    juce::ValueTree captureDocument() override        { return doc.toValueTree(); }
    void applyDocument (const juce::ValueTree& t) override { doc = MyDoc::fromValueTree (t); /* push to engine, tell the UI */ }
    juce::File presetFolder() const override          { return /* the plugin's setting, or PresetFile::defaultPresetFolder */; }
    void setPresetFolder (const juce::File& f) override { /* persist it */ }
    void documentSaved (const juce::File&, const atl::PresetMetadata& m) override { /* name/author now m's */ }

    // Whether the live document has edits not yet written to its file. Drives the load-over-dirty
    // guard and the pinned strip's state chip.
    bool documentHasUnsavedChanges() const override   { return editor.isDirty(); }
};
```

`documentHasUnsavedChanges()` defaults to `true`. That is deliberate: a host that has not
implemented it gets the load-over-dirty prompt on every load — annoying, but never destructive,
which is the right failure mode for a flag the panel cannot compute itself (it never infers
dirtiness by comparing trees, and it polls the flag a few times a second, so the answer needs to be
a cheap read, not a computation). Implement it as soon as the plugin can say, truthfully, whether
the live document differs from its file.

`PresetHost::migratePresetFolder (dest, format)` also has a working default: it moves every preset
file with `PresetFile::moveAllPresets` and adopts `dest` only if every file moved. Most adapters
never need to touch it. Override it when the plugin keeps something else in its preset folder that
has to travel with the presets, or when it remembers the open preset's *path* somewhere outside
`PresetHost` — the panel follows a moved file for its own bookkeeping (`currentFile`, the
selection), but nothing tells a plugin-side path to follow along unless the override does it.

The adapter is the only place the librarian and the plugin meet. `PresetPanel::setColours` takes
the plugin's palette so the tab does not look like a guest — it declares `background`, `panel`,
`text`, `textDim`, `accent`, `outline`, plus `warning` (the unsaved-changes chip), `danger` (text on
a destructive button) and `favourite` (the star); everything else the panel paints is derived from
those.

## Built-in presets

A plugin that ships presets inside its binary hands them to the library once:

```cpp
library.setBuiltIns ({ { "Bell", "ReadyMade", BinaryData::Bell_lanes, BinaryData::Bell_lanesSize } });
```

They list, filter, load and duplicate like any other row. The one thing they cannot do is be
written to: `renamePreset` and `deletePreset` refuse. In the panel, `delete` is disabled on a
built-in row, and `edit… → apply` doesn't edit it in place — it writes an editable copy into the
user's preset folder, under the name in the popover, carrying the edited metadata. Saving a patch
that was loaded from a built-in behaves the same way: a built-in has no file of its own to write
back to, so save always creates a new file under the live patch's name rather than touching the
built-in. Duplicating a built-in is the direct route to the same result.

The panel grows a **built-in / mine** pair of toggles by itself, but only in a plugin that actually
has built-ins.

## Tags

Every tag is a first-class object, not a text field, and there are four places it shows up:

- **The tag rail**, on the left: every tag in the library with a count. Clicking one toggles it as
  a filter.
- **Row chips**: each preset's tags are shown on its row; clicking one toggles that tag as a filter
  too, so filtering is available wherever the tag was noticed.
- **Drag to assign**: dragging a row onto a rail tag assigns it, and the status bar confirms
  (`tagged "glocken" as bell`, or says it was already tagged). A built-in can't be tagged this way
  — dropping one on a tag reports `"x" is a factory preset — duplicate it to tag it` rather than
  failing quietly. Duplicate it first, then tag the copy.
- **The tag picker**, in the edit popover: the precise path. Chips with a `×` to remove, and an
  input that adds an existing tag or creates a new one on Return or a comma (so `a, b, c` typed or
  pasted adds three) — typing a spelling that differs
  only in case joins the existing tag rather than starting a second one.

Tags ignore case everywhere, not just in the picker: files tagged `Bass` and `bass` (say, tagged by
hand or by an older version) are one tag on the rail, counted together and spelt the way the scan
first met it; filtering on either matches both, and dropping `bass` on a row tagged `Bass` is
"already tagged".

Active tag filters combine as `match all` (AND, the default) or `match any` (OR), shown as
removable chips in their own row once any tag is filtering, with a `clear` to drop them all. They
combine with the text filter and the built-in/mine source toggles the same way `PresetFilter` does.

## Favourites

The star on the left of a row is a favourite: one click, no dialog, no file write. Favourites are
kept by the panel, not stored in the preset — starring is a preference about a file, not a property
of it, and starring a built-in must not force a copy of it into existence. `PresetPanel::getFavourites()`
/ `setFavourites()` hand a plugin the current list (opaque strings) and take one back; `onFavouritesChanged`
says when to re-read it. A plugin that wants favourites to survive a restart persists what it gets
and hands it back at startup — the panel does nothing on its own to make them outlast the editor.
The keys stay stable across a folder migration, so favourites keep pointing at the right presets
after their files move.

## Migrating the preset folder

`folder…` only repoints the library — nothing moves, and the status bar says so. `migrate…` is the
two-step move:

1. **Migrate preset folder** — pick a destination; a summary line says how many user presets will
   move (factory presets never do — they stay in the plugin), and `move presets` stays disabled
   until the destination is chosen and differs from the source.
2. **Moved N presets** — the library now points at the new folder. If the old one is now empty of
   anything but empty directories, this step offers `keep it` or `delete old folder`
   (destructive-styled, never the emphasised option); if anything else is still in there — files
   that are not presets — deleting is not offered at all, only `keep it`, and the leftover names are
   listed. Deleting is never implied by migrating.

The old folder, when deleted, goes to the system trash, never a hard delete. A failed move stops at
the first failure, reports what already moved, and leaves the library pointed at the old folder.

## Panel options

`PresetPanel::setOptions (PresetPanelOptions)` — every field has a default that matches the design;
change one only with a reason.

| Option | Default | Effect |
| --- | --- | --- |
| `showTagRail` | `true` | show or hide the left tag rail |
| `showComments` | `true` | show the comment line under each row |
| `loadGuard` | `dirty` | `dirty` / `always` / `none` — see "Browsing is not loading" above |
| `confirmDelete` | `true` | ask before a delete, rather than deleting on click |

## Listing what only the plugin understands

`PresetMetadata::extra` is a `juce::ValueTree` the librarian stores inside `<PresetMeta>` and never
looks inside. It is scanned with the rest of the metadata, which is the point: it is for the columns
of a list — a duration, a one-shot flag — not for the document. Anything needed only *after* loading
belongs in the document tree instead.

Filtering on it is `PresetFilter::extraPredicate`, applied after the text/tag/source filtering:

```cpp
filter.extraPredicate = [] (const PresetEntry& e)
    { return (double) e.meta.extra.getProperty ("durationSeconds", 0.0) < 2.0; };
```

## Optional panel pieces

None of these are needed for a plain librarian.

- `PresetPanel::setExtras` takes a `PresetPanelExtras`: a `detailsHeader` component that sits inside
  the edit popover, above `file name` (a waveform, a thumbnail — shown only while the popover is
  open), an `extraFilters` component that sits in its own row under the filter bar (the plugin's own
  filter controls), and the `extraFilterPredicate` those controls drive. Components are borrowed,
  not owned. Call `refilter()` when they change.
- `PresetPanel::setAuditioner` takes a `PresetAuditioner` and turns on an audition button beside
  `load` on the selected row, driven by the selection — the answer to rule 3's "how do I hear it
  before I commit to it". The librarian never learns that audition means audio — it asks whether a
  row can be previewed and says when to start and stop. Changing rows, and closing the panel, stop
  whatever was previewing.

## Tests

Tests are `juce::UnitTest` classes with no runner of their own, in three files — a consuming
project adds `${ATL_PRESETS_TEST_SOURCES}` to its own test binary (see above), so there is one
runner per build tree rather than one per library.

`Tests/PresetFileTests.cpp` covers the naming rule, the metadata round-trip, a failed save not
clobbering the file it was overwriting, the scanner, rename / duplicate / delete, the `extra`
tree, and built-ins — loading from memory, key stability, the source toggles and the extra
predicate, and the refuse-to-write / duplicate-instead rules. `Tests/PresetLibraryTests.cpp` covers
filtering (including `matchAnyTag`), tag counts, every sort key in both directions, and
`moveAllPresets` — collisions, refusing a destination inside the source, and when a source folder
still counts as having "other content" after a move.

`Tests/PresetPanelTests.cpp` drives a real `PresetPanel`, and is the one that needs a
`juce::ScopedJuceInitialiser_GUI` alive in the consuming test binary, since it builds actual
`juce::Component`s. It checks: that selecting a row never loads it; that Load, Return, a row's
`load` chip (once, even when double-clicked) and a double-click all fire `documentLoaded` and never `documentSaved` (and that a double-click on a row
that isn't the selection loads nothing); the dirty guard's three answers (`cancel` loads nothing,
`load anyway` loads, `save, then load` writes the open preset back first) and that `loadGuard`
changes the policy; the live strip's save and revert; naming and saving a patch that has never been
saved; the row popover's rename-by-apply and its tag picker; drag-to-assign onto a tag; tag filters
combining as match-all and match-any; favourites; delete's confirmation and trash; migrate's
two-step folder move; and the built-in rules (no delete, `apply` writes a copy, saving a patch
loaded from a built-in writes a new file under its name). Panel dialogs — the load guard, delete
confirmation, migrate — are components drawn inside the panel rather than native windows, so a test
answers them by pressing a button, synchronously, with no message loop to pump.

## Where it came from

Extracted from the Sampler plugin (`~/dev/Sampler`), which had it as `Source/Patch/PatchFile`,
`Source/Patch/PatchLibrary` and `Source/UI/PresetPanel`, themselves ported down from
`messy system plugin`. The Sampler is the first consumer, and `Source/Patch/SamplerPresetHost.h`
there is a worked example of the adapter. The Lanes plugin (`~/dev/lanes-audio-plugin`) is the
second, and is what built-ins, `PresetMetadata::extra`, the panel extras and the auditioner hook
were added for.

A new consumer should start from whichever adapter is closer: Sampler's implements only the base
`PresetHost` virtuals (the defaults exist so a plain consumer never needs the richer API); Lanes'
uses all of it -- built-ins, `extra` metadata, panel extras, the auditioner. Favourites and the
"open preset" path are the plugin's to persist; the panel keeps neither beyond the editor.
