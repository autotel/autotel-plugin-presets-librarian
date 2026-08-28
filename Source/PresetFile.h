#pragma once
#include <juce_data_structures/juce_data_structures.h>
#include <optional>

namespace atl
{

/** What one plugin's preset files are: the extension they carry and the shape of the tree inside.

    The librarian is deliberately ignorant of what a document *means*. It reads and writes a
    `juce::ValueTree` of `rootType` with a `<PresetMeta>` child appended, and that is the whole
    contract -- which is what lets the same panel serve a sampler, a synth and an effect without
    any of them leaking into it.

    `nameProperty`/`authorProperty` are the root properties the librarian keeps in step with the
    file: the name is stamped from the filename on every save, the author from the metadata. A
    plugin whose document tree already carries those under different names says so here rather than
    growing a translation layer. */
struct PresetFormat
{
    juce::String     extension      { ".preset" };   // dot included
    juce::Identifier rootType       { "Document" };
    juce::Identifier nameProperty   { "name" };
    juce::Identifier authorProperty { "author" };
};

/** User-facing metadata for a saved preset.

    **A preset's name is its filename.** `name` is the file's base name, not a field stored
    separately: `save()` stamps it into the tree it writes, and `load()`/`scanMetadataOnly()` read
    it back off the file. One name, in one place -- so renaming a file in a file manager renames the
    preset, and there is never a second name to reconcile.

    `comment` is the annotation a filename cannot carry -- "this is the newer one", "delete this" --
    shown beside the name in the list and never part of it. */
struct PresetMetadata
{
    juce::String      name, author, description, comment;
    juce::StringArray tags;
    juce::String      date;   // ISO-8601, stamped at save time

    /** Whatever else a plugin wants listed without loading the document: a duration, a
        one-shot/looped flag, a thumbnail. Stored inside `<PresetMeta>` and round-tripped
        untouched -- the librarian never reads a property of it, so a plugin can put anything here
        and still be listed by a build that knows nothing about it.

        Scanned along with the rest, which is the point: it is for the columns of a list, not for
        the document. Anything the plugin needs only after loading belongs in the document tree. */
    juce::ValueTree extra;

    /** Only what has no home in the document tree: description, comment, tags, date, extra. */
    juce::ValueTree toValueTree() const;
    static PresetMetadata fromValueTree (const juce::ValueTree& presetMetaTree,
                                         const juce::String& nameFromFile,
                                         const juce::String& authorFromTree);
};

/** Reads and writes preset files: a document `juce::ValueTree` with a `<PresetMeta>` child, written
    with `juce::ValueTree::writeToStream`. Binary rather than XML because a librarian may re-read a
    whole folder on a refresh, and because a preset file is not meant to be hand-edited.

    Free of `juce_gui_basics` and of any `juce::AudioProcessor`, so it links into a headless test
    binary. */
namespace PresetFile
{
    struct Loaded
    {
        juce::ValueTree tree;
        PresetMetadata  meta;
    };

    /** Writes `document` plus `meta` to `file`. The document's name property is set from **the
        file's own base name** -- not from `meta.name` -- so the two cannot drift apart. Creates the
        parent directory if needed. False on any I/O failure. */
    bool save (const juce::File& file, juce::ValueTree document, const PresetMetadata& meta,
               const PresetFormat& format);

    /** Reads a preset file. The loaded tree's name property is the file's base name, so an
        external rename is a rename of the preset. `nullopt` for a file that is missing, empty,
        unreadable, or not a well-formed tree of the expected root type -- callers should treat
        that as "skip this file", never as a crash. */
    std::optional<Loaded> load (const juce::File& file, const PresetFormat& format);

    /** Enough to list a folder without materialising the document. Still parses the tree once (the
        binary format has no skip-children shortcut), but skips whatever the plugin does with it. */
    std::optional<PresetMetadata> scanMetadataOnly (const juce::File& file, const PresetFormat& format);

    /** The same as `load`, for a preset the plugin ships inside its own binary rather than on
        disk. There is no filename to take the name from, so `name` is passed in -- the resource's
        name, which is the built-in equivalent of a file's. `data` is read, never retained. */
    std::optional<Loaded> loadFromData (const void* data, int sizeInBytes, const juce::String& name,
                                        const PresetFormat& format);

    /** `scanMetadataOnly` for a built-in. See `loadFromData`. */
    std::optional<PresetMetadata> scanMetadataOnlyFromData (const void* data, int sizeInBytes,
                                                            const juce::String& name,
                                                            const PresetFormat& format);

    /** A `Presets` subfolder of wherever the plugin keeps its global settings. */
    juce::File defaultPresetFolder (const juce::File& settingsDir);

    /** Strips '/ \ : * ? " < > |', collapses whitespace runs, trims, caps at 100 chars. Returns
        "Untitled" if the result would otherwise be empty. */
    juce::String sanitiseFilename (const juce::String& input);

    /** Appends " 2", " 3", ... to `baseName` until the file does not exist, or is `ignoreFile`
        (allowed to "collide" with itself -- used when re-saving a file in place). */
    juce::File resolveCollision (const juce::File& folder, const juce::String& baseName,
                                 const PresetFormat& format, const juce::File& ignoreFile = {});
}

} // namespace atl
