#include "PresetLibrary.h"

namespace atl
{

PresetLibrary::PresetLibrary (PresetFormat format, juce::File presetDir)
    : format_ (std::move (format)), presetDir_ (std::move (presetDir)) {}

void PresetLibrary::rescan()
{
    entries_.clear();
    if (! presetDir_.isDirectory()) return;

    const auto pattern = juce::String ("*") + format_.extension;
    for (auto& f : presetDir_.findChildFiles (juce::File::findFiles, true, pattern))
    {
        auto meta = PresetFile::scanMetadataOnly (f, format_);
        if (! meta.has_value()) continue;   // unreadable or foreign -- skip, don't fail the scan

        PresetEntry e;
        e.file = f;
        e.meta = *meta;
        entries_.push_back (std::move (e));
    }
}

juce::StringArray PresetLibrary::getAllTags() const
{
    juce::StringArray tags;
    for (const auto& e : entries_)
        for (const auto& t : e.meta.tags)
            if (! tags.contains (t)) tags.add (t);
    tags.sort (true);
    return tags;
}

std::vector<const PresetEntry*> PresetLibrary::getFiltered (const PresetFilter& filter) const
{
    std::vector<const PresetEntry*> out;

    for (const auto& e : entries_)
    {
        if (! filter.requiredTags.isEmpty())
        {
            bool hasAll = true;
            for (const auto& t : filter.requiredTags)
                if (! e.meta.tags.contains (t)) { hasAll = false; break; }
            if (! hasAll) continue;
        }

        if (filter.text.isNotEmpty())
        {
            const auto needle = filter.text.toLowerCase();
            bool matches = e.meta.name.toLowerCase().contains (needle)
                          || e.meta.author.toLowerCase().contains (needle)
                          || e.meta.comment.toLowerCase().contains (needle)
                          || e.meta.description.toLowerCase().contains (needle);
            if (! matches)
                for (const auto& t : e.meta.tags)
                    if (t.toLowerCase().contains (needle)) { matches = true; break; }
            if (! matches) continue;
        }

        out.push_back (&e);
    }

    return out;
}

bool PresetLibrary::deletePreset (const PresetEntry& entry, juce::String& errorOut)
{
    if (! entry.file.existsAsFile()) { errorOut = "File no longer exists."; return false; }

    bool ok = entry.file.moveToTrash();
    if (! ok) ok = entry.file.deleteFile();

    if (! ok)
    {
        errorOut = "Could not delete \"" + entry.file.getFullPathName() + "\".";
        return false;
    }

    rescan();
    return true;
}

bool PresetLibrary::renamePreset (PresetEntry& entry, const PresetMetadata& newMeta,
                                  juce::String& errorOut)
{
    if (! entry.file.existsAsFile()) { errorOut = "File no longer exists."; return false; }

    auto loaded = PresetFile::load (entry.file, format_);
    if (! loaded.has_value()) { errorOut = "Could not read the file."; return false; }

    auto meta = newMeta;
    meta.date = juce::Time::getCurrentTime().toISO8601 (true);

    const auto wanted = PresetFile::sanitiseFilename (newMeta.name);
    const bool nameChanged = wanted != entry.file.getFileNameWithoutExtension();
    const auto target = nameChanged
        ? PresetFile::resolveCollision (entry.file.getParentDirectory(), wanted, format_, entry.file)
        : entry.file;

    if (! PresetFile::save (target, loaded->tree, meta, format_))
    {
        errorOut = "Could not write \"" + target.getFullPathName() + "\".";
        return false;
    }

    if (target != entry.file && entry.file.existsAsFile())
        entry.file.deleteFile();

    entry.file = target;
    entry.meta = meta;
    entry.meta.name = target.getFileNameWithoutExtension();   // the file had the last word

    rescan();
    return true;
}

bool PresetLibrary::duplicatePreset (const PresetEntry& entry, juce::File& newFileOut,
                                     juce::String& errorOut)
{
    if (! entry.file.existsAsFile()) { errorOut = "File no longer exists."; return false; }

    const auto target = PresetFile::resolveCollision (entry.file.getParentDirectory(),
                                                      entry.file.getFileNameWithoutExtension(),
                                                      format_);
    if (! entry.file.copyFileTo (target))
    {
        errorOut = "Could not write \"" + target.getFullPathName() + "\".";
        return false;
    }

    newFileOut = target;
    rescan();
    return true;
}

} // namespace atl
