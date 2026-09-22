#pragma once
#include "PresetFile.h"
#include <functional>
#include <vector>

namespace atl
{

/** One row: either a file on disk, or a preset the plugin ships inside its own binary.

    A built-in is a row like any other -- it lists, filters, loads and duplicates the same way. The
    only thing it cannot do is be written to, which is what `isBuiltIn` gates: rename and delete
    refuse, and Save on the panel turns into Save As. */
struct PresetEntry
{
    juce::File     file;          // invalid for a built-in
    PresetMetadata meta;

    bool         isBuiltIn = false;
    juce::String category;        // optional grouping shown beside the name; built-ins usually set it

    /** Built-ins only: bytes with a lifetime longer than this library's (BinaryData's arrays are
        static, which is the case this exists for). Never freed here. */
    const void* data = nullptr;
    int         dataSize = 0;

    /** Stable identity for re-selecting the same row across a rescan. */
    juce::String key() const
    {
        return isBuiltIn ? "builtin:" + category + "/" + meta.name
                         : file.getFullPathName();
    }
};

/** A preset the plugin ships inside its binary. `data` must outlive the library. */
struct BuiltInPreset
{
    juce::String name;       // stands in for a filename -- see PresetFile::loadFromData
    juce::String category;
    const void*  data = nullptr;
    int          size = 0;
};

/** The name a row shows: the file's base name, or a built-in's resource name. The filename is the
    name (see `PresetMetadata`), so this is never `meta.name` for a file. */
juce::String displayName (const PresetEntry&);

/** Free text over name/author/description/comment, plus required tags. */
struct PresetFilter
{
    juce::String      text;
    juce::StringArray requiredTags;   // an entry must carry ALL of these -- or ANY, see below

    /** `requiredTags` as OR rather than AND: an entry carrying any one of them passes. Irrelevant
        while `requiredTags` is empty. */
    bool matchAnyTag = false;

    bool includeBuiltIn = true;
    bool includeUser    = true;

    /** The last word, applied after everything above. A plugin that filters on something only it
        understands -- a duration, a one-shot flag out of `PresetMetadata::extra` -- puts it here
        rather than growing this struct a field the other plugins would ignore. */
    std::function<bool (const PresetEntry&)> extraPredicate;
};

/** One tag and how many rows carry it -- what the panel's tag rail lists. */
struct TagCount
{
    juce::String tag;
    int count = 0;
};

/** What a list can be ordered by. The secondary key is always the name, so the order is stable
    whatever the primary key does. */
enum class PresetSortKey
{
    name,              // display name, natural and case-insensitive ("kit 2" before "kit 10")
    dateModified,      // `meta.date`; a file with none falls back to its modification time
    author,
    firstTag,          // alphabetically first of the row's tags; untagged rows sort after tagged
    favouritesFirst,   // favourites are the *greater* key: descending puts them on top
    source             // built-ins before the user's own when ascending
};

/** Orders `rows` in place. `ascending` reverses the whole comparison, secondary key included.
    `isFavourite` is only consulted by `favouritesFirst`; left empty, nothing is a favourite. */
void sortPresets (std::vector<const PresetEntry*>& rows, PresetSortKey key, bool ascending,
                  const std::function<bool (const PresetEntry&)>& isFavourite = {});

/** Scans a folder of preset files (recursively) into rows and offers filtered views over them,
    plus the file operations a librarian needs: delete, rename, duplicate. Knows nothing about what
    a document contains -- see `PresetFormat`.

    Free of `juce_gui_basics`, so it links into a headless test binary. */
class PresetLibrary
{
public:
    explicit PresetLibrary (PresetFormat format, juce::File presetDir = {});

    /** The presets shipped inside the plugin binary. Listed ahead of whatever is in the folder, and
        re-added by every `rescan()` -- so this is set once, not per scan. */
    void setBuiltIns (std::vector<BuiltInPreset> builtIns);

    void setPresetDir (const juce::File& dir) { presetDir_ = dir; }
    const juce::File& getPresetDir() const { return presetDir_; }
    const PresetFormat& getFormat() const { return format_; }

    /** Rescans the folder. Unreadable or foreign files are skipped, never fatal to the scan. */
    void rescan();

    const std::vector<PresetEntry>& getAll() const { return entries_; }
    /** Bumped by every rescan -- including the ones inside `renamePreset`, `deletePreset` and
        `writeEditableCopy`. A pointer from `getAll()` or `getFiltered()` taken under an older
        generation points into freed storage. */
    int generation() const { return generation_; }
    std::vector<const PresetEntry*> getFiltered (const PresetFilter&) const;
    juce::StringArray getAllTags() const;

    /** Every tag in the library (built-ins included, filters ignored) with the number of rows
        carrying it, ordered by tag, case-insensitively. */
    std::vector<TagCount> getTagCounts() const;

    /** Moves the file to the trash (falling back to a hard delete). The caller is expected to have
        confirmed with the user already. Refuses a built-in. */
    bool deletePreset (const PresetEntry& entry, juce::String& errorOut);

    /** Rewrites the preset's metadata and, when `newMeta.name` differs from **the file's** base
        name, renames the file. Compared against the file rather than the stored metadata, so an
        external rename settles instead of bouncing back. `entry` is overwritten with the result.
        Refuses a built-in. */
    bool renamePreset (PresetEntry& entry, const PresetMetadata& newMeta, juce::String& errorOut);

    /** Copies the preset under the next free name ("kit" -> "kit 2"). A byte copy rather than a
        load-and-save: a duplicate should be the same file even if the host build cannot parse every
        corner of it.

        A built-in duplicates too, and that is the point -- it is how a read-only preset becomes an
        editable one. The copy lands in the preset folder under the built-in's own name, so the
        first duplicate of "Bell" is "Bell" and not "Bell 2" unless one is already there. */
    bool duplicatePreset (const PresetEntry& entry, juce::File& newFileOut, juce::String& errorOut);

    /** Editing a preset that cannot be written to -- a built-in -- writes an editable copy instead
        of failing: `duplicatePreset`, then `renamePreset` on the copy with `newMeta`. `copyOut` is
        the copy as it ended up on disk. If the metadata step fails the plain duplicate is left in
        place (it is still a faithful copy) and false is returned. Works on a writable entry too,
        where it is simply "duplicate, then edit the duplicate". */
    bool writeEditableCopy (const PresetEntry& entry, const PresetMetadata& newMeta,
                            PresetEntry& copyOut, juce::String& errorOut);

private:
    void rescanBuiltIns();
    void rescanFolder();

    PresetFormat format_;
    juce::File presetDir_;
    std::vector<BuiltInPreset> builtIns_;
    std::vector<PresetEntry> entries_;
    int generation_ = 0;
};

} // namespace atl
