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

/** What moving a preset folder did -- see `PresetFile::moveAllPresets`. Enough for the panel to say
    what happened without re-reading either folder. */
struct MigrationResult
{
    /** Every preset file moved. False means the move stopped at the first failure: the files in
        `moved` are already at their new home, everything else is still where it was. */
    bool ok = false;

    /** Where each moved file now lives, in the order they were moved. */
    juce::Array<juce::File> moved;

    /** Where each file in `moved` came from -- same index, same order. What a caller holding a
        reference to one of the old files (the open preset, a favourite) needs to follow it. */
    juce::Array<juce::File> movedFrom;

    /** Files left behind on purpose because they are not preset files -- a readme, a sample, a
        `.DS_Store`. Paths relative to the source folder. */
    juce::StringArray skipped;

    /** The first failure, when `ok` is false. */
    juce::String error;

    /** Whether anything other than empty directories is still in the source folder afterwards --
        skipped files, or presets a failed move did not get to. An old folder with something left
        in it is not offered for deletion. */
    bool sourceHasOtherContent = false;
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

    /** How many files carrying `format.extension` are under `folder`, recursively -- what a
        migration would move. Counts by extension only, the same way `moveAllPresets` picks. */
    int countPresetFiles (const juce::File& folder, const PresetFormat& format);

    /** Moves every file carrying `format.extension` under `from` (recursively, keeping each one's
        subfolder relative to `from`) into `to`, one file at a time. Anything else is left where it
        is and listed in `skipped`. A name already taken in the destination is resolved with
        `resolveCollision` rather than overwritten. Stops at the first failure.

        Never deletes `from`, and never touches anything outside the two folders. Refuses (with
        `ok` false and nothing moved) when `to` is `from`, or is inside it, or when `from` is not a
        directory. Creates `to` if needed. */
    MigrationResult moveAllPresets (const juce::File& from, const juce::File& to,
                                    const PresetFormat& format);
}

} // namespace atl
