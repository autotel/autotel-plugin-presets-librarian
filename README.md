# Autotel plugin presets librarian

The preset/patch librarian shared by the Autotel plugins: one file format, one folder scanner, one
panel, so saving and browsing presets works the same way in every one of them.

## What it is

- `atl::PresetFile` — reads and writes a preset file: a `juce::ValueTree` document with a
  `<PresetMeta>` child appended, written with `ValueTree::writeToStream`.
- `atl::PresetLibrary` — scans a folder into rows, filters them, and does delete / rename /
  duplicate.
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

## Tests

`Tests/PresetFileTests.cpp` holds `juce::UnitTest` classes with no runner of their own — a consuming
project adds `${ATL_PRESETS_TEST_SOURCES}` to its own test binary (see above). They cover the
naming rule, the metadata round-trip, the scanner, and rename / duplicate / delete.

## Where it came from

Extracted from the Sampler plugin (`~/dev/Sampler`), which had it as `Source/Patch/PatchFile`,
`Source/Patch/PatchLibrary` and `Source/UI/PresetPanel`, themselves ported down from
`messy system plugin`. The Sampler is the first consumer, and `Source/Patch/SamplerPresetHost.h`
there is a worked example of the adapter.
