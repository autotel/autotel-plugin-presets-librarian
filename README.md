# Autotel plugin presets librarian

The preset/patch librarian shared by the Autotel plugins: one file format, one folder scanner, one
panel, so saving and browsing presets works the same way in every one of them.

## What it is

- `atl::PresetFile` — reads and writes a preset file: a `juce::ValueTree` document with a
  `<PresetMeta>` child appended, written with `ValueTree::writeToStream`. Also reads one straight
  out of memory, for presets a plugin ships inside its own binary.
- `atl::PresetLibrary` — scans a folder into rows, filters them, and does delete / rename /
  duplicate. Built-in presets list alongside the folder's.
- `atl::PresetPanel` — the librarian tab: filtered list, details column, save / save as / rename /
  delete, and a row context menu.

Two rules the rest of the design follows from:

1. **A preset's name is its filename.** `save()` stamps the file's base name into the document it
   writes; `load()` reads the name back off the file. Renaming a file in a file manager renames the
   preset, and there is never a second name to reconcile. The annotation a filename cannot carry
   goes in `comment`, which is shown in the list and is never part of a name.
2. **The librarian does not know what a document means.** It moves `juce::ValueTree`s around. What
   a plugin's tree contains, and what happens when one is loaded, is the plugin's business.

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
};
```

The adapter is the only place the librarian and the plugin meet. `PresetPanel::setColours` takes the
plugin's palette so the tab does not look like a guest.

## Built-in presets

A plugin that ships presets inside its binary hands them to the library once:

```cpp
library.setBuiltIns ({ { "Bell", "ReadyMade", BinaryData::Bell_lanes, BinaryData::Bell_lanesSize } });
```

They list, filter, load and duplicate like any other row. The one thing they cannot do is be written
to: `renamePreset` and `deletePreset` refuse, the panel disables Rename and Delete, and Save falls
through to Save As — so editing a built-in produces a copy in the user's folder rather than a
failure. Duplicating one is the deliberate route to an editable version, and the copy takes the
built-in's own name.

The panel grows a **built-in / mine** pair of toggles by itself, but only in a plugin that actually
has built-ins.

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

Neither is needed for a plain librarian.

- `PresetPanel::setExtras` takes a `PresetPanelExtras`: a component to sit at the top of the details
  column (a waveform, a thumbnail), a component to sit under the search row (the plugin's own filter
  controls), and the `extraFilterPredicate` those controls drive. Components are borrowed, not owned.
  Call `refilter()` when they change.
- `PresetPanel::setAuditioner` takes a `PresetAuditioner` and turns on an audition button driven by
  the selected row. The librarian never learns that audition means audio — it asks whether a row can
  be previewed and says when to start and stop.

## Tests

`Tests/PresetFileTests.cpp` holds `juce::UnitTest` classes with no runner of their own — a consuming
project adds `${ATL_PRESETS_TEST_SOURCES}` to its own test binary (see above). They cover the
naming rule, the metadata round-trip, the scanner, rename / duplicate / delete, the `extra` tree,
and built-ins: loading from memory, key stability, the source toggles and the extra predicate, and
the refuse-to-write / duplicate-instead rules.

## Where it came from

Extracted from the Sampler plugin (`~/dev/Sampler`), which had it as `Source/Patch/PatchFile`,
`Source/Patch/PatchLibrary` and `Source/UI/PresetPanel`, themselves ported down from
`messy system plugin`. The Sampler is the first consumer, and `Source/Patch/SamplerPresetHost.h`
there is a worked example of the adapter. The Lanes plugin (`~/dev/lanes-audio-plugin`) is the
second, and is what built-ins, `PresetMetadata::extra`, the panel extras and the auditioner hook
were added for.
