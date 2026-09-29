#include "PresetLibrary.h"
#include <algorithm>

namespace atl
{

juce::String displayName (const PresetEntry& entry)
{
    // Read off the file itself, not meta.name -- meta.name can be stale after an external rename
    // until the next rescan, and the file is the thing that is actually true right now.
    return entry.isBuiltIn ? entry.meta.name : entry.file.getFileNameWithoutExtension();
}

PresetLibrary::PresetLibrary (PresetFormat format, juce::File presetDir)
    : format_ (std::move (format)), presetDir_ (std::move (presetDir)) {}

void PresetLibrary::setBuiltIns (std::vector<BuiltInPreset> builtIns)
{
    builtIns_ = std::move (builtIns);
}

void PresetLibrary::rescan()
{
    ++generation_;
    entries_.clear();
    rescanBuiltIns();
    rescanFolder();
}

void PresetLibrary::rescanBuiltIns()
{
    for (const auto& b : builtIns_)
    {
        auto meta = PresetFile::scanMetadataOnlyFromData (b.data, b.size, b.name, format_);
        if (! meta.has_value()) continue;   // a built-in this build cannot parse is still just a skip

        PresetEntry e;
        e.isBuiltIn = true;
        e.category = b.category;
        e.data = b.data;
        e.dataSize = b.size;
        e.meta = *meta;
        entries_.push_back (std::move (e));
    }
}

void PresetLibrary::rescanFolder()
{
    if (! presetDir_.isDirectory()) return;

    const auto pattern = juce::String ("*") + format_.extension;
    for (auto& f : presetDir_.findChildFiles (juce::File::findFiles, true, pattern))
    {
        auto meta = PresetFile::scanMetadataOnly (f, format_);
        if (! meta.has_value()) continue;   // unreadable or foreign -- skip, don't fail the scan

        PresetEntry e;
        e.file = f;
        e.meta = *meta;
        // A folder under the preset dir reads as a category, the way it already reads as one to
        // anybody looking at the folder in a file manager.
        e.category = f.getParentDirectory() == presetDir_
                        ? juce::String()
                        : f.getParentDirectory().getRelativePathFrom (presetDir_);
        entries_.push_back (std::move (e));
    }
}

juce::StringArray PresetLibrary::getAllTags() const
{
    juce::StringArray tags;
    for (const auto& e : entries_)
        for (const auto& t : e.meta.tags)
            if (! tags.contains (t, true)) tags.add (t);
    tags.sort (true);
    return tags;
}

std::vector<TagCount> PresetLibrary::getTagCounts() const
{
    std::vector<TagCount> counts;

    for (const auto& e : entries_)
        for (const auto& t : e.meta.tags)
        {
            auto existing = std::find_if (counts.begin(), counts.end(),
                                          [&] (const TagCount& c) { return c.tag.equalsIgnoreCase (t); });
            if (existing == counts.end())
                counts.push_back ({ t, 1 });
            else
                ++existing->count;
        }

    std::sort (counts.begin(), counts.end(), [] (const TagCount& a, const TagCount& b)
    {
        return a.tag.compareIgnoreCase (b.tag) < 0;
    });

    return counts;
}

void sortPresets (std::vector<const PresetEntry*>& rows, PresetSortKey key, bool ascending,
                  const std::function<bool (const PresetEntry&)>& isFavourite)
{
    // meta.date if it has one; a file with none falls back to its own modification time. A
    // built-in with no date has neither, and sorts as the empty string.
    auto dateOf = [] (const PresetEntry& e) -> juce::String
    {
        if (e.meta.date.isNotEmpty()) return e.meta.date;
        if (! e.isBuiltIn && e.file.existsAsFile())
            return e.file.getLastModificationTime().toISO8601 (true);
        return {};
    };

    // Alphabetically first, case-insensitively; empty (untagged) is handled by the caller.
    auto firstTagOf = [] (const PresetEntry& e) -> juce::String
    {
        juce::String lowest;
        for (const auto& t : e.meta.tags)
            if (lowest.isEmpty() || t.compareIgnoreCase (lowest) < 0)
                lowest = t;
        return lowest;
    };

    std::stable_sort (rows.begin(), rows.end(), [&] (const PresetEntry* a, const PresetEntry* b)
    {
        int primary = 0;
        switch (key)
        {
            case PresetSortKey::name:
                // Falls straight through to the secondary (name) key below -- it is the same
                // comparison, so there is no point computing it twice.
                break;

            case PresetSortKey::dateModified:
                primary = dateOf (*a).compare (dateOf (*b));
                break;

            case PresetSortKey::author:
                primary = a->meta.author.compareIgnoreCase (b->meta.author);
                break;

            case PresetSortKey::firstTag:
            {
                const auto ta = firstTagOf (*a);
                const auto tb = firstTagOf (*b);
                const bool aUntagged = ta.isEmpty();
                const bool bUntagged = tb.isEmpty();
                if (aUntagged != bUntagged)
                    primary = aUntagged ? 1 : -1;   // untagged sorts after tagged
                else
                    primary = ta.compareIgnoreCase (tb);
                break;
            }

            case PresetSortKey::favouritesFirst:
            {
                const bool fa = isFavourite && isFavourite (*a);
                const bool fb = isFavourite && isFavourite (*b);
                primary = fa == fb ? 0 : (fa ? 1 : -1);   // favourite is the greater key
                break;
            }

            case PresetSortKey::source:
                // Built-in is the lesser key -- built-ins sort first when ascending.
                primary = a->isBuiltIn == b->isBuiltIn ? 0 : (a->isBuiltIn ? -1 : 1);
                break;
        }

        const int cmp = primary != 0 ? primary
                                     : displayName (*a).compareNatural (displayName (*b), false);
        return ascending ? cmp < 0 : cmp > 0;
    });
}

std::vector<const PresetEntry*> PresetLibrary::getFiltered (const PresetFilter& filter) const
{
    std::vector<const PresetEntry*> out;

    for (const auto& e : entries_)
    {
        if (e.isBuiltIn ? ! filter.includeBuiltIn : ! filter.includeUser) continue;

        if (! filter.requiredTags.isEmpty())
        {
            bool passes;
            if (filter.matchAnyTag)
            {
                passes = false;
                for (const auto& t : filter.requiredTags)
                    if (e.meta.tags.contains (t, true)) { passes = true; break; }
            }
            else
            {
                passes = true;
                for (const auto& t : filter.requiredTags)
                    if (! e.meta.tags.contains (t, true)) { passes = false; break; }
            }
            if (! passes) continue;
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

        if (filter.extraPredicate && ! filter.extraPredicate (e)) continue;

        out.push_back (&e);
    }

    return out;
}

bool PresetLibrary::deletePreset (const PresetEntry& entry, juce::String& errorOut)
{
    if (entry.isBuiltIn) { errorOut = "Built-in presets cannot be deleted."; return false; }
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
    if (entry.isBuiltIn)
    {
        errorOut = "Built-in presets cannot be renamed or edited -- duplicate it first.";
        return false;
    }
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
    // A built-in has no folder of its own, so its copy goes to the preset folder -- which is also
    // the only writable place it could go.
    if (entry.isBuiltIn)
    {
        if (entry.data == nullptr || entry.dataSize <= 0)
        {
            errorOut = "This built-in has no data to copy.";
            return false;
        }
        if (! presetDir_.isDirectory() && ! presetDir_.createDirectory().wasOk())
        {
            errorOut = "Could not create \"" + presetDir_.getFullPathName() + "\".";
            return false;
        }

        const auto target = PresetFile::resolveCollision (presetDir_, entry.meta.name, format_);
        if (! target.replaceWithData (entry.data, (size_t) entry.dataSize))
        {
            errorOut = "Could not write \"" + target.getFullPathName() + "\".";
            return false;
        }

        newFileOut = target;
        rescan();
        return true;
    }

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

bool PresetLibrary::writeEditableCopy (const PresetEntry& entry, const PresetMetadata& newMeta,
                                       PresetEntry& copyOut, juce::String& errorOut)
{
    juce::File newFile;
    if (! duplicatePreset (entry, newFile, errorOut))
        return false;

    // A value, not a reference into entries_: renamePreset below calls rescan(), which would
    // leave a reference into the vector dangling the moment it clears and repopulates.
    PresetEntry copy;
    bool found = false;
    for (const auto& e : entries_)
        if (! e.isBuiltIn && e.file == newFile) { copy = e; found = true; break; }

    if (! found)
    {
        errorOut = "The copy could not be found after duplicating.";
        return false;
    }

    // renamePreset writes whatever meta it is given, so the extra tree has to be carried over
    // explicitly: newMeta's own if the caller set one, otherwise the copy's existing one (a
    // faithful duplicate already has one worth keeping).
    auto metaToWrite = newMeta;
    if (! metaToWrite.extra.isValid())
        metaToWrite.extra = copy.meta.extra;

    if (! renamePreset (copy, metaToWrite, errorOut))
        return false;   // the plain duplicate is left in place -- still a faithful copy

    copyOut = copy;
    return true;
}

} // namespace atl
