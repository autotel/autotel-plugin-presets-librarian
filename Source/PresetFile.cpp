#include "PresetFile.h"

namespace atl
{

namespace
{
namespace id
{
#define ATL_ID(x) static const juce::Identifier x { #x };
ATL_ID (PresetMeta) ATL_ID (description) ATL_ID (comment) ATL_ID (tags) ATL_ID (date)
ATL_ID (PresetExtra)
#undef ATL_ID
}

/** Parses the root tree out of raw bytes. Returns an invalid tree (rather than throwing) for
    anything that is not a well-formed binary ValueTree of the expected root type, so a garbled or
    foreign file is "skip", not "crash". */
juce::ValueTree parseRoot (const juce::MemoryBlock& data, const PresetFormat& format)
{
    if (data.getSize() == 0) return {};

    juce::ValueTree root;
    try
    {
        root = juce::ValueTree::readFromData (data.getData(), data.getSize());
    }
    catch (...)
    {
        return {};
    }

    if (! root.isValid() || ! root.hasType (format.rootType))
        return {};
    return root;
}

std::optional<juce::ValueTree> readRootFromFile (const juce::File& file, const PresetFormat& format)
{
    if (! file.existsAsFile()) return std::nullopt;

    juce::MemoryBlock data;
    if (! file.loadFileAsData (data) || data.getSize() == 0) return std::nullopt;

    auto root = parseRoot (data, format);
    if (! root.isValid()) return std::nullopt;
    return root;
}
} // namespace

juce::ValueTree PresetMetadata::toValueTree() const
{
    juce::ValueTree t { id::PresetMeta };
    t.setProperty (id::description, description, nullptr);
    t.setProperty (id::comment, comment, nullptr);
    t.setProperty (id::tags, tags.joinIntoString (","), nullptr);
    t.setProperty (id::date, date, nullptr);

    // A copy, and wrapped in a node of the librarian's own naming: the caller's tree must not
    // acquire a parent as a side effect of being described, and whatever type the plugin gave it
    // must not collide with a child the librarian may add later.
    if (extra.isValid())
    {
        juce::ValueTree wrapper { id::PresetExtra };
        wrapper.appendChild (extra.createCopy(), nullptr);
        t.appendChild (wrapper, nullptr);
    }
    return t;
}

PresetMetadata PresetMetadata::fromValueTree (const juce::ValueTree& presetMetaTree,
                                              const juce::String& nameFromFile,
                                              const juce::String& authorFromTree)
{
    PresetMetadata m;
    m.name = nameFromFile;
    m.author = authorFromTree;

    if (presetMetaTree.isValid() && presetMetaTree.hasType (id::PresetMeta))
    {
        m.description = presetMetaTree.getProperty (id::description, "").toString();
        m.comment = presetMetaTree.getProperty (id::comment, "").toString();
        m.tags = juce::StringArray::fromTokens (presetMetaTree.getProperty (id::tags, "").toString(),
                                                ",", "");
        m.tags.trim();
        m.tags.removeEmptyStrings();
        m.date = presetMetaTree.getProperty (id::date, "").toString();

        if (auto wrapper = presetMetaTree.getChildWithName (id::PresetExtra); wrapper.isValid()
                                                                              && wrapper.getNumChildren() > 0)
            m.extra = wrapper.getChild (0).createCopy();
    }

    if (m.date.isEmpty())
        m.date = juce::Time::getCurrentTime().toISO8601 (true);

    return m;
}

namespace PresetFile
{

bool save (const juce::File& file, juce::ValueTree document, const PresetMetadata& meta,
           const PresetFormat& format)
{
    if (! document.isValid()) return false;

    auto parent = file.getParentDirectory();
    if (! parent.isDirectory() && ! parent.createDirectory().wasOk())
        return false;

    // A copy: the caller's live document must not acquire this file's name as a side effect of
    // being saved. The name comes from the file, never from `meta` -- see PresetMetadata.
    auto root = document.createCopy();
    root.setProperty (format.nameProperty, file.getFileNameWithoutExtension(), nullptr);
    root.setProperty (format.authorProperty, meta.author, nullptr);

    // One <PresetMeta> per file: re-saving a tree that already carries one would otherwise stack
    // them up, and `getChildWithName` would keep answering with the oldest.
    root.removeChild (root.getChildWithName (id::PresetMeta), nullptr);
    root.appendChild (meta.toValueTree(), nullptr);

    juce::TemporaryFile temp (file);
    {
        juce::FileOutputStream out (temp.getFile());
        if (! out.openedOk()) return false;
        root.writeToStream (out);
    }
    return temp.overwriteTargetFileWithTemporary();
}

namespace
{
/** Shared by the file and the in-memory paths: the only thing that differs between a preset on
    disk and one shipped inside the binary is where the name comes from. */
Loaded finishLoad (juce::ValueTree root, const juce::String& name, const PresetFormat& format)
{
    // The name from outside the tree wins. A name inside it only stands in for a tree with no file
    // behind it -- a host state chunk, or something hand-assembled.
    if (name.isNotEmpty())
        root.setProperty (format.nameProperty, name, nullptr);

    Loaded out;
    out.meta = PresetMetadata::fromValueTree (root.getChildWithName (id::PresetMeta),
                                              root.getProperty (format.nameProperty, "").toString(),
                                              root.getProperty (format.authorProperty, "").toString());
    // Handed over without the librarian's own child: the plugin gets its document back as it
    // wrote it, not with a node it has to know to ignore.
    root.removeChild (root.getChildWithName (id::PresetMeta), nullptr);
    out.tree = root;
    return out;
}

PresetMetadata finishScan (const juce::ValueTree& root, const juce::String& name,
                           const PresetFormat& format)
{
    const auto resolved = name.isNotEmpty() ? name
                                            : root.getProperty (format.nameProperty, "").toString();
    return PresetMetadata::fromValueTree (root.getChildWithName (id::PresetMeta), resolved,
                                          root.getProperty (format.authorProperty, "").toString());
}
} // namespace

std::optional<Loaded> load (const juce::File& file, const PresetFormat& format)
{
    auto root = readRootFromFile (file, format);
    if (! root.has_value()) return std::nullopt;
    return finishLoad (*root, file.getFileNameWithoutExtension(), format);
}

std::optional<PresetMetadata> scanMetadataOnly (const juce::File& file, const PresetFormat& format)
{
    auto root = readRootFromFile (file, format);
    if (! root.has_value()) return std::nullopt;
    return finishScan (*root, file.getFileNameWithoutExtension(), format);
}

std::optional<Loaded> loadFromData (const void* data, int sizeInBytes, const juce::String& name,
                                    const PresetFormat& format)
{
    if (data == nullptr || sizeInBytes <= 0) return std::nullopt;

    auto root = parseRoot (juce::MemoryBlock (data, (size_t) sizeInBytes), format);
    if (! root.isValid()) return std::nullopt;
    return finishLoad (root, name, format);
}

std::optional<PresetMetadata> scanMetadataOnlyFromData (const void* data, int sizeInBytes,
                                                        const juce::String& name,
                                                        const PresetFormat& format)
{
    if (data == nullptr || sizeInBytes <= 0) return std::nullopt;

    auto root = parseRoot (juce::MemoryBlock (data, (size_t) sizeInBytes), format);
    if (! root.isValid()) return std::nullopt;
    return finishScan (root, name, format);
}

juce::File defaultPresetFolder (const juce::File& settingsDir)
{
    return settingsDir.getChildFile ("Presets");
}

juce::String sanitiseFilename (const juce::String& input)
{
    static const juce::String illegal ("/\\:*?\"<>|");

    juce::String stripped;
    for (auto c : input)
        if (illegal.indexOfChar (c) < 0)
            stripped += juce::String::charToString (c);

    juce::String collapsed;
    bool lastWasSpace = false;
    for (auto c : stripped)
    {
        const bool isSpace = juce::CharacterFunctions::isWhitespace (c);
        if (isSpace)
        {
            if (! lastWasSpace) collapsed += " ";
            lastWasSpace = true;
        }
        else
        {
            collapsed += juce::String::charToString (c);
            lastWasSpace = false;
        }
    }

    collapsed = collapsed.trim();
    if (collapsed.length() > 100) collapsed = collapsed.substring (0, 100).trim();
    if (collapsed.isEmpty()) collapsed = "Untitled";
    return collapsed;
}

juce::File resolveCollision (const juce::File& folder, const juce::String& baseName,
                             const PresetFormat& format, const juce::File& ignoreFile)
{
    auto candidate = folder.getChildFile (baseName + format.extension);
    if (! candidate.exists() || candidate == ignoreFile)
        return candidate;

    for (int n = 2; n < 1000; ++n)
    {
        candidate = folder.getChildFile (baseName + " " + juce::String (n) + format.extension);
        if (! candidate.exists() || candidate == ignoreFile)
            return candidate;
    }
    return candidate;
}

} // namespace PresetFile
} // namespace atl
