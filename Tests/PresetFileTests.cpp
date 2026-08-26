#include "PresetLibrary.h"
#include "PresetPanel.h"
#include <juce_core/juce_core.h>

using namespace atl;

namespace
{
/** The format a test plugin would declare. Nothing here knows what "Doc" contains, which is the
    point of the librarian. */
PresetFormat testFormat()
{
    return { ".atltest", juce::Identifier ("Doc"), juce::Identifier ("name"),
             juce::Identifier ("author") };
}

juce::ValueTree makeDocument()
{
    juce::ValueTree doc { "Doc" };
    doc.setProperty ("name", "whatever the tree says", nullptr);
    doc.setProperty ("author", "ignored on load", nullptr);

    juce::ValueTree child { "thing" };
    child.setProperty ("value", 42, nullptr);
    doc.appendChild (child, nullptr);
    return doc;
}

PresetMetadata makeMeta()
{
    PresetMetadata m;
    m.name = "asked for";
    m.author = "someone";
    m.description = "a description";
    m.comment = "<< this is the newer one";
    m.tags = { "drums", "punchy" };
    m.date = "2026-01-01T00:00:00";
    return m;
}

struct PresetFileTests : juce::UnitTest
{
    PresetFileTests() : juce::UnitTest ("atl::PresetFile", "presets") {}

    void runTest() override
    {
        const auto format = testFormat();
        auto tempDir = juce::File::createTempFile ("atl-presets-tests");
        tempDir.deleteFile();
        tempDir.createDirectory();

        beginTest ("a document round-trips, and comes back without the librarian's own child");
        {
            const auto file = tempDir.getChildFile ("round trip" + format.extension);
            expect (PresetFile::save (file, makeDocument(), makeMeta(), format));

            const auto loaded = PresetFile::load (file, format);
            expect (loaded.has_value());
            expectEquals (loaded->tree.getNumChildren(), 1);
            expectEquals ((int) loaded->tree.getChild (0).getProperty ("value"), 42);
            expect (! loaded->tree.getChildWithName ("PresetMeta").isValid(),
                    "the plugin gets its document back, not a tree with a node to ignore");

            expectEquals (loaded->meta.author, juce::String ("someone"));
            expectEquals (loaded->meta.description, juce::String ("a description"));
            expectEquals (loaded->meta.comment, juce::String ("<< this is the newer one"));
            expectEquals (loaded->meta.tags.joinIntoString (","), juce::String ("drums,punchy"));
        }

        beginTest ("the filename is the name, whatever the tree or the caller says");
        {
            const auto file = tempDir.getChildFile ("on disk" + format.extension);
            expect (PresetFile::save (file, makeDocument(), makeMeta(), format));

            const auto loaded = PresetFile::load (file, format);
            expect (loaded.has_value());
            expectEquals (loaded->meta.name, juce::String ("on disk"));
            expectEquals (loaded->tree.getProperty ("name").toString(), juce::String ("on disk"));
            expectEquals (PresetFile::scanMetadataOnly (file, format)->name, juce::String ("on disk"));

            // An external rename is a rename of the preset: nothing inside the file changes.
            const auto moved = tempDir.getChildFile ("renamed by hand" + format.extension);
            expect (file.moveFileTo (moved));
            expectEquals (PresetFile::load (moved, format)->meta.name,
                          juce::String ("renamed by hand"));
        }

        beginTest ("saving does not rename the caller's live document");
        {
            auto live = makeDocument();
            const auto file = tempDir.getChildFile ("side effects" + format.extension);
            expect (PresetFile::save (file, live, makeMeta(), format));
            expectEquals (live.getProperty ("name").toString(), juce::String ("whatever the tree says"));
        }

        beginTest ("re-saving a loaded document keeps exactly one metadata node");
        {
            const auto file = tempDir.getChildFile ("resaved" + format.extension);
            expect (PresetFile::save (file, makeDocument(), makeMeta(), format));

            auto first = PresetFile::load (file, format);
            expect (first.has_value());
            auto second = makeMeta();
            second.comment = "edited";
            expect (PresetFile::save (file, first->tree, second, format));

            const auto again = PresetFile::load (file, format);
            expect (again.has_value());
            expectEquals (again->meta.comment, juce::String ("edited"));
        }

        beginTest ("scanMetadataOnly agrees with a full load");
        {
            const auto file = tempDir.getChildFile ("scan only" + format.extension);
            expect (PresetFile::save (file, makeDocument(), makeMeta(), format));

            const auto scanned = PresetFile::scanMetadataOnly (file, format);
            const auto loaded = PresetFile::load (file, format);
            expect (scanned.has_value() && loaded.has_value());
            expectEquals (scanned->name, loaded->meta.name);
            expectEquals (scanned->author, loaded->meta.author);
            expectEquals (scanned->comment, loaded->meta.comment);
            expectEquals (scanned->tags.joinIntoString (","), loaded->meta.tags.joinIntoString (","));
        }

        beginTest ("a missing, empty, garbled or foreign file is skipped, not fatal");
        {
            const auto missing = tempDir.getChildFile ("nope" + format.extension);
            expect (! PresetFile::load (missing, format).has_value());
            expect (! PresetFile::scanMetadataOnly (missing, format).has_value());

            const auto garbage = tempDir.getChildFile ("garbage" + format.extension);
            garbage.replaceWithText ("not a preset, just text {}[]<>");
            expect (! PresetFile::load (garbage, format).has_value());

            const auto empty = tempDir.getChildFile ("empty" + format.extension);
            empty.create();
            expect (! PresetFile::load (empty, format).has_value());

            // A well-formed tree of somebody else's root type is not this plugin's preset.
            const auto foreign = tempDir.getChildFile ("foreign" + format.extension);
            {
                juce::ValueTree other { "SomeoneElsesDocument" };
                juce::FileOutputStream out (foreign);
                other.writeToStream (out);
            }
            expect (! PresetFile::load (foreign, format).has_value());
        }

        beginTest ("sanitiseFilename strips what a filesystem will not take");
        {
            expectEquals (PresetFile::sanitiseFilename ("a/b:c*d"), juce::String ("abcd"));
            expectEquals (PresetFile::sanitiseFilename ("  spaced   out  "), juce::String ("spaced out"));
            expectEquals (PresetFile::sanitiseFilename ("   "), juce::String ("Untitled"));
        }

        beginTest ("resolveCollision finds a free name and honours ignoreFile");
        {
            const auto a = tempDir.getChildFile ("collide" + format.extension);
            a.create();
            expectEquals (PresetFile::resolveCollision (tempDir, "collide", format).getFileName(),
                          juce::String ("collide 2") + format.extension);
            expectEquals (PresetFile::resolveCollision (tempDir, "collide", format, a).getFullPathName(),
                          a.getFullPathName());
        }

        beginTest ("the library lists, filters and tags what is in its folder");
        {
            auto dir = tempDir.getChildFile ("library");
            dir.createDirectory();

            auto kick = makeMeta();
            kick.tags = { "drums" };
            PresetFile::save (dir.getChildFile ("Kick Kit" + format.extension), makeDocument(), kick,
                              format);

            auto pad = makeMeta();
            pad.tags = { "pad", "ambient" };
            pad.comment = "warm";
            PresetFile::save (dir.getChildFile ("Ambient Pad" + format.extension), makeDocument(),
                              pad, format);

            dir.getChildFile ("notes.txt").replaceWithText ("hello");   // ignored

            PresetLibrary library (format, dir);
            library.rescan();
            expectEquals ((int) library.getAll().size(), 2);

            PresetFilter byName;
            byName.text = "kick";
            expectEquals ((int) library.getFiltered (byName).size(), 1);

            // The comment is searchable too -- it is the note people actually leave.
            PresetFilter byComment;
            byComment.text = "warm";
            expectEquals ((int) library.getFiltered (byComment).size(), 1);
            expectEquals (library.getFiltered (byComment)[0]->meta.name, juce::String ("Ambient Pad"));

            PresetFilter byTag;
            byTag.requiredTags = { "ambient" };
            expectEquals ((int) library.getFiltered (byTag).size(), 1);

            const auto tags = library.getAllTags();
            expect (tags.contains ("drums") && tags.contains ("pad") && tags.contains ("ambient"));
        }

        beginTest ("an absent folder yields no entries and no crash");
        {
            PresetLibrary library (format, tempDir.getChildFile ("not-a-folder"));
            library.rescan();
            expect (library.getAll().empty());
        }

        beginTest ("rename rewrites the metadata and moves the file");
        {
            auto dir = tempDir.getChildFile ("renames");
            dir.createDirectory();
            const auto file = dir.getChildFile ("original" + format.extension);
            PresetFile::save (file, makeDocument(), makeMeta(), format);

            PresetLibrary library (format, dir);
            library.rescan();
            auto entry = library.getAll()[0];

            auto meta = entry.meta;
            meta.name = "renamed";
            meta.tags = { "new-tag" };

            juce::String err;
            expect (library.renamePreset (entry, meta, err));
            expect (err.isEmpty());
            expectEquals (entry.meta.name, juce::String ("renamed"));
            expect (entry.file.existsAsFile());
            expect (! file.existsAsFile(), "the old filename is gone");

            const auto reloaded = PresetFile::load (entry.file, format);
            expectEquals (reloaded->meta.name, juce::String ("renamed"));
            expectEquals (reloaded->meta.tags.joinIntoString (","), juce::String ("new-tag"));
            expectEquals (reloaded->tree.getChild (0).getProperty ("value").toString(),
                          juce::String ("42"));   // the document itself survived the rename
        }

        beginTest ("renaming to the name it already has is not a rename");
        {
            auto dir = tempDir.getChildFile ("no-op-renames");
            dir.createDirectory();
            const auto file = dir.getChildFile ("steady" + format.extension);
            PresetFile::save (file, makeDocument(), makeMeta(), format);

            PresetLibrary library (format, dir);
            library.rescan();
            auto entry = library.getAll()[0];

            auto meta = entry.meta;
            meta.comment = "just the comment changed";

            juce::String err;
            expect (library.renamePreset (entry, meta, err));
            expectEquals (entry.file.getFullPathName(), file.getFullPathName());
            expectEquals ((int) library.getAll().size(), 1, "no second file appeared");
            expectEquals (PresetFile::load (file, format)->meta.comment,
                          juce::String ("just the comment changed"));
        }

        beginTest ("duplicate copies under the next free name; delete removes the file");
        {
            auto dir = tempDir.getChildFile ("dupes");
            dir.createDirectory();
            PresetFile::save (dir.getChildFile ("kit" + format.extension), makeDocument(),
                              makeMeta(), format);

            PresetLibrary library (format, dir);
            library.rescan();

            juce::File copy;
            juce::String err;
            expect (library.duplicatePreset (library.getAll()[0], copy, err));
            expectEquals (copy.getFileName(), juce::String ("kit 2") + format.extension);
            expectEquals ((int) library.getAll().size(), 2);
            // Named by its own file, like every other preset.
            expectEquals (PresetFile::load (copy, format)->meta.name, juce::String ("kit 2"));

            PresetEntry theCopy;
            for (const auto& e : library.getAll())
                if (e.file == copy) theCopy = e;

            expect (library.deletePreset (theCopy, err));
            expect (! copy.existsAsFile());
            expectEquals ((int) library.getAll().size(), 1);
        }

        tempDir.deleteRecursively();
    }
};

static PresetFileTests presetFileTests;

/** A minimal PresetHost recording which of documentLoaded()/documentSaved() fired and with what,
    so a plugin's "show the name outside the panel" logic has something to assert against. */
struct RecordingHost : PresetHost
{
    juce::ValueTree document { "Doc" };

    juce::ValueTree captureDocument() override { return document; }
    void applyDocument (const juce::ValueTree& t) override { document = t; }
    juce::File presetFolder() const override { return folder; }
    void setPresetFolder (const juce::File& f) override { folder = f; }

    void documentSaved (const juce::File& f, const PresetMetadata& m) override
    {
        savedCount++; lastFile = f; lastMeta = m;
    }
    void documentLoaded (const juce::File& f, const PresetMetadata& m) override
    {
        loadedCount++; lastFile = f; lastMeta = m;
    }

    juce::File folder;
    int savedCount = 0, loadedCount = 0;
    juce::File lastFile;
    PresetMetadata lastMeta;
};

/** Drives a real PresetPanel the way a click on its list would -- selectRow() on the ListBox
    calls its model's selectedRowsChanged() synchronously (see juce_ListBox.cpp), which is
    PresetPanel's own load path (ListModel::selectedRowsChanged -> selectionChanged ->
    loadEntry()). `list`/`loadEntry()` are private, so this walks the panel's real child
    components for the one juce::ListBox it owns rather than adding a test-only accessor. */
struct PresetPanelHookTests : juce::UnitTest
{
    PresetPanelHookTests() : juce::UnitTest ("atl::PresetPanel", "presets") {}

    void runTest() override
    {
        const auto format = testFormat();
        auto tempDir = juce::File::createTempFile ("atl-presetpanel-tests");
        tempDir.deleteFile();
        tempDir.createDirectory();

        PresetFile::save (tempDir.getChildFile ("kit" + format.extension), makeDocument(),
                          makeMeta(), format);

        RecordingHost host;
        host.folder = tempDir;

        PresetPanel panel (host, format);
        panel.setSize (400, 300);

        juce::ListBox* list = nullptr;
        for (int i = 0; i < panel.getNumChildComponents() && list == nullptr; ++i)
            list = dynamic_cast<juce::ListBox*> (panel.getChildComponent (i));

        beginTest ("selecting a row fires documentLoaded, not documentSaved");
        {
            expect (list != nullptr, "PresetPanel must own exactly one juce::ListBox");
            if (list == nullptr) return;

            list->selectRow (0);

            expectEquals (host.loadedCount, 1);
            expectEquals (host.savedCount, 0);
            expectEquals (host.lastFile.getFileNameWithoutExtension(), juce::String ("kit"));
            expectEquals (host.lastMeta.name, juce::String ("kit"));
        }

        tempDir.deleteRecursively();
    }
};

static PresetPanelHookTests presetPanelHookTests;
} // namespace
