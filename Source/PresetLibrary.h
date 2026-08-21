#pragma once
#include "PresetFile.h"
#include <vector>

namespace atl
{

/** One row: a file on disk plus the metadata scanned out of it. */
struct PresetEntry
{
    juce::File     file;
    PresetMetadata meta;

    /** Stable identity for re-selecting the same row across a rescan. */
    juce::String key() const { return file.getFullPathName(); }
};

/** Free text over name/author/description/comment, plus required tags. */
struct PresetFilter
{
    juce::String      text;
    juce::StringArray requiredTags;   // an entry must carry ALL of these
};

/** Scans a folder of preset files (recursively) into rows and offers filtered views over them,
    plus the file operations a librarian needs: delete, rename, duplicate. Knows nothing about what
    a document contains -- see `PresetFormat`.

    Free of `juce_gui_basics`, so it links into a headless test binary. */
class PresetLibrary
{
public:
    explicit PresetLibrary (PresetFormat format, juce::File presetDir = {});

    void setPresetDir (const juce::File& dir) { presetDir_ = dir; }
    const juce::File& getPresetDir() const { return presetDir_; }
    const PresetFormat& getFormat() const { return format_; }

    /** Rescans the folder. Unreadable or foreign files are skipped, never fatal to the scan. */
    void rescan();

    const std::vector<PresetEntry>& getAll() const { return entries_; }
    std::vector<const PresetEntry*> getFiltered (const PresetFilter&) const;
    juce::StringArray getAllTags() const;

    /** Moves the file to the trash (falling back to a hard delete). The caller is expected to have
        confirmed with the user already. */
    bool deletePreset (const PresetEntry& entry, juce::String& errorOut);

    /** Rewrites the preset's metadata and, when `newMeta.name` differs from **the file's** base
        name, renames the file. Compared against the file rather than the stored metadata, so an
        external rename settles instead of bouncing back. `entry` is overwritten with the result. */
    bool renamePreset (PresetEntry& entry, const PresetMetadata& newMeta, juce::String& errorOut);

    /** Copies the preset beside itself under the next free name ("kit" -> "kit 2"). A byte copy
        rather than a load-and-save: a duplicate should be the same file even if the host build
        cannot parse every corner of it. */
    bool duplicatePreset (const PresetEntry& entry, juce::File& newFileOut, juce::String& errorOut);

private:
    PresetFormat format_;
    juce::File presetDir_;
    std::vector<PresetEntry> entries_;
};

} // namespace atl
