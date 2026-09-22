#include "PresetLibrary.h"
#include <juce_core/juce_core.h>

using namespace atl;

namespace
{

/** The format a test plugin would declare. Nothing here knows what "Doc" contains, which is the
    point of the librarian. Its own copy -- see PresetFileTests.cpp -- because these helpers are
    per translation unit. */
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

/** Saves a preset with a given name/tags/date straight into `dir`, sidestepping the library so a
    test can set up an exact starting layout. */
juce::File savePreset (const juce::File& dir, const juce::String& name,
                       const juce::StringArray& tags, const juce::String& date,
                       const PresetFormat& format)
{
    auto meta = makeMeta();
    meta.tags = tags;
    meta.date = date;
    const auto file = dir.getChildFile (name + format.extension);
    PresetFile::save (file, makeDocument(), meta, format);
    return file;
}

struct PresetLibrarySortAndFilterTests : juce::UnitTest
{
    PresetLibrarySortAndFilterTests() : juce::UnitTest ("atl::PresetLibrary sort & filter", "presets") {}

    void runTest() override
    {
        const auto format = testFormat();
        auto tempDir = juce::File::createTempFile ("atl-library-tests");
        tempDir.deleteFile();
        tempDir.createDirectory();

        beginTest ("displayName reads the file, not meta.name, and a built-in's resource name");
        {
            auto dir = tempDir.getChildFile ("display-name");
            dir.createDirectory();
            savePreset (dir, "on disk", {}, "", format);

            PresetLibrary library (format, dir);
            library.rescan();
            expectEquals (displayName (library.getAll()[0]), juce::String ("on disk"));

            PresetEntry builtIn;
            builtIn.isBuiltIn = true;
            builtIn.meta.name = "Bell";
            expectEquals (displayName (builtIn), juce::String ("Bell"));
        }

        beginTest ("matchAnyTag is OR, the default is AND, and the text/source filters still combine");
        {
            auto dir = tempDir.getChildFile ("tag-match");
            dir.createDirectory();
            savePreset (dir, "Kick", { "drums", "punchy" }, "", format);
            savePreset (dir, "Snare", { "drums" }, "", format);
            savePreset (dir, "Pad", { "ambient" }, "", format);

            PresetLibrary library (format, dir);
            library.rescan();

            PresetFilter matchAll;
            matchAll.requiredTags = { "drums", "punchy" };
            expectEquals ((int) library.getFiltered (matchAll).size(), 1,
                          "AND (the default) needs every tag");
            expectEquals (displayName (*library.getFiltered (matchAll)[0]), juce::String ("Kick"));

            PresetFilter matchAny;
            matchAny.requiredTags = { "drums", "ambient" };
            matchAny.matchAnyTag = true;
            expectEquals ((int) library.getFiltered (matchAny).size(), 3,
                          "OR needs only one of the tags");

            // Text and matchAnyTag combine rather than either one winning outright.
            PresetFilter textAndAnyTag;
            textAndAnyTag.text = "kick";
            textAndAnyTag.requiredTags = { "drums", "ambient" };
            textAndAnyTag.matchAnyTag = true;
            expectEquals ((int) library.getFiltered (textAndAnyTag).size(), 1);
            expectEquals (displayName (*library.getFiltered (textAndAnyTag)[0]), juce::String ("Kick"));
        }

        beginTest ("getTagCounts counts rows and orders tags case-insensitively");
        {
            auto dir = tempDir.getChildFile ("tag-counts");
            dir.createDirectory();
            savePreset (dir, "a", { "Drums" }, "", format);
            savePreset (dir, "b", { "drums" }, "", format);
            savePreset (dir, "c", { "ambient" }, "", format);
            savePreset (dir, "d", { "Bells" }, "", format);

            PresetLibrary library (format, dir);
            library.rescan();

            const auto counts = library.getTagCounts();
            juce::StringArray order;
            for (const auto& c : counts) order.add (c.tag);
            expectEquals (order.joinIntoString (","), juce::String ("ambient,Bells,Drums,drums"));

            for (const auto& c : counts)
                if (c.tag == "ambient")
                    expectEquals (c.count, 1);
        }

        beginTest ("each sort key orders both directions, with a stable natural-order name tiebreak");
        {
            auto dir = tempDir.getChildFile ("sort-name");
            dir.createDirectory();
            savePreset (dir, "kit 10", {}, "", format);
            savePreset (dir, "kit 2", {}, "", format);
            savePreset (dir, "Apple", {}, "", format);

            PresetLibrary library (format, dir);
            library.rescan();

            auto rows = library.getFiltered ({});
            sortPresets (rows, PresetSortKey::name, true);
            juce::StringArray names;
            for (auto* r : rows) names.add (displayName (*r));
            expectEquals (names.joinIntoString (","), juce::String ("Apple,kit 2,kit 10"),
                          "natural order: kit 2 before kit 10, not lexicographic");

            sortPresets (rows, PresetSortKey::name, false);
            names.clear();
            for (auto* r : rows) names.add (displayName (*r));
            expectEquals (names.joinIntoString (","), juce::String ("kit 10,kit 2,Apple"));
        }

        beginTest ("dateModified sorts by meta.date, falling back to file modification time");
        {
            auto dir = tempDir.getChildFile ("sort-date");
            dir.createDirectory();
            savePreset (dir, "old", {}, "2020-01-01T00:00:00", format);
            savePreset (dir, "new", {}, "2025-01-01T00:00:00", format);
            savePreset (dir, "mid", {}, "2022-01-01T00:00:00", format);

            PresetLibrary library (format, dir);
            library.rescan();

            auto rows = library.getFiltered ({});
            sortPresets (rows, PresetSortKey::dateModified, true);
            juce::StringArray names;
            for (auto* r : rows) names.add (displayName (*r));
            expectEquals (names.joinIntoString (","), juce::String ("old,mid,new"));

            sortPresets (rows, PresetSortKey::dateModified, false);
            names.clear();
            for (auto* r : rows) names.add (displayName (*r));
            expectEquals (names.joinIntoString (","), juce::String ("new,mid,old"));
        }

        beginTest ("author sorts case-insensitively with the name tiebreak");
        {
            auto dir = tempDir.getChildFile ("sort-author");
            dir.createDirectory();
            auto a = makeMeta(); a.author = "zed"; a.name = "one";
            PresetFile::save (dir.getChildFile ("one" + format.extension), makeDocument(), a, format);
            auto b = makeMeta(); b.author = "Amy"; b.name = "two";
            PresetFile::save (dir.getChildFile ("two" + format.extension), makeDocument(), b, format);
            auto c = makeMeta(); c.author = "amy"; c.name = "aardvark";
            PresetFile::save (dir.getChildFile ("aardvark" + format.extension), makeDocument(), c,
                              format);

            PresetLibrary library (format, dir);
            library.rescan();

            auto rows = library.getFiltered ({});
            sortPresets (rows, PresetSortKey::author, true);
            juce::StringArray names;
            for (auto* r : rows) names.add (displayName (*r));
            // Both "Amy"s tie on author (case-insensitively) and fall back to natural name order.
            expectEquals (names.joinIntoString (","), juce::String ("aardvark,two,one"));
        }

        beginTest ("firstTag is the row's alphabetically-lowest tag, case-insensitively; untagged is last");
        {
            auto dir = tempDir.getChildFile ("sort-firsttag");
            dir.createDirectory();
            savePreset (dir, "none", {}, "", format);
            savePreset (dir, "zzz-first", { "Zebra", "amber" }, "", format);
            savePreset (dir, "aaa-first", { "aardvark" }, "", format);

            PresetLibrary library (format, dir);
            library.rescan();

            auto rows = library.getFiltered ({});
            sortPresets (rows, PresetSortKey::firstTag, true);
            juce::StringArray names;
            for (auto* r : rows) names.add (displayName (*r));
            expectEquals (names.joinIntoString (","),
                          juce::String ("aaa-first,zzz-first,none"),
                          "aardvark < amber (the lowest of zzz-first's tags), untagged sorts last");

            sortPresets (rows, PresetSortKey::firstTag, false);
            names.clear();
            for (auto* r : rows) names.add (displayName (*r));
            expectEquals (names.joinIntoString (","), juce::String ("none,zzz-first,aaa-first"),
                          "descending still puts untagged last-to-first, i.e. first");
        }

        beginTest ("favouritesFirst uses the predicate, and descending puts favourites on top");
        {
            auto dir = tempDir.getChildFile ("sort-favourites");
            dir.createDirectory();
            savePreset (dir, "Apple", {}, "", format);
            savePreset (dir, "Banana", {}, "", format);
            savePreset (dir, "Cherry", {}, "", format);

            PresetLibrary library (format, dir);
            library.rescan();

            auto isFavourite = [] (const PresetEntry& e) { return displayName (e) == "Banana"; };

            auto rows = library.getFiltered ({});
            sortPresets (rows, PresetSortKey::favouritesFirst, true, isFavourite);
            juce::StringArray names;
            for (auto* r : rows) names.add (displayName (*r));
            expectEquals (names.joinIntoString (","), juce::String ("Apple,Cherry,Banana"),
                          "ascending: favourite is the greater key, so it sorts last");

            sortPresets (rows, PresetSortKey::favouritesFirst, false, isFavourite);
            names.clear();
            for (auto* r : rows) names.add (displayName (*r));
            // Descending reverses the whole comparison, secondary key included -- so the
            // non-favourites' name order flips too, not just the favourite/non-favourite split.
            expectEquals (names.joinIntoString (","), juce::String ("Banana,Cherry,Apple"),
                          "descending: the favourite is on top");
        }

        beginTest ("source sorts built-ins before the user's own when ascending");
        {
            auto dir = tempDir.getChildFile ("sort-source");
            dir.createDirectory();
            savePreset (dir, "Mine", {}, "", format);

            PresetMetadata builtInMeta;
            builtInMeta.date = "2026-01-01T00:00:00";
            const auto builtInFile = dir.getChildFile ("staging" + format.extension);
            PresetFile::save (builtInFile, makeDocument(), builtInMeta, format);
            juce::MemoryBlock builtInData;
            builtInFile.loadFileAsData (builtInData);
            builtInFile.deleteFile();

            PresetLibrary library (format, dir);
            library.setBuiltIns ({ { "Built", "Factory", builtInData.getData(),
                                     (int) builtInData.getSize() } });
            library.rescan();

            auto rows = library.getFiltered ({});
            sortPresets (rows, PresetSortKey::source, true);
            expect (rows[0]->isBuiltIn, "ascending: built-in first");

            sortPresets (rows, PresetSortKey::source, false);
            expect (! rows[0]->isBuiltIn, "descending: the user's own first");
        }

        beginTest ("writeEditableCopy on a built-in copies into the preset folder, untouched original");
        {
            auto dir = tempDir.getChildFile ("edit-builtin");
            dir.createDirectory();

            PresetMetadata builtInMeta;
            builtInMeta.tags = { "factory" };
            builtInMeta.date = "2026-01-01T00:00:00";
            const auto stagingFile = dir.getChildFile ("staging" + format.extension);
            PresetFile::save (stagingFile, makeDocument(), builtInMeta, format);
            juce::MemoryBlock builtInData;
            stagingFile.loadFileAsData (builtInData);
            stagingFile.deleteFile();

            PresetLibrary library (format, dir);
            library.setBuiltIns ({ { "Bell", "ReadyMade", builtInData.getData(),
                                     (int) builtInData.getSize() } });
            library.rescan();

            auto builtIn = library.getAll()[0];
            expect (builtIn.isBuiltIn);

            auto newMeta = builtIn.meta;
            newMeta.name = "My Bell";
            newMeta.comment = "edited";

            PresetEntry copyOut;
            juce::String err;
            expect (library.writeEditableCopy (builtIn, newMeta, copyOut, err), err);
            expect (! copyOut.isBuiltIn);
            expect (copyOut.file.existsAsFile());
            expect (copyOut.file.getParentDirectory() == dir);
            expectEquals (copyOut.meta.name, juce::String ("My Bell"));
            expectEquals (copyOut.file.getFileNameWithoutExtension(), juce::String ("My Bell"));

            const auto reloaded = PresetFile::load (copyOut.file, format);
            expect (reloaded.has_value());
            expectEquals (reloaded->meta.comment, juce::String ("edited"));
            // The built-in's own data is never touched by any of this.
            const auto stillBuiltIn = PresetFile::loadFromData (builtInData.getData(),
                                                                (int) builtInData.getSize(), "Bell",
                                                                format);
            expect (stillBuiltIn.has_value());
            expect (stillBuiltIn->meta.comment.isEmpty());

            // The library still lists exactly one built-in and one writable copy.
            library.rescan();
            int builtInCount = 0, writableCount = 0;
            for (const auto& e : library.getAll())
                (e.isBuiltIn ? builtInCount : writableCount)++;
            expectEquals (builtInCount, 1);
            expectEquals (writableCount, 1);
        }

        beginTest ("writeEditableCopy on a user preset duplicates then edits the duplicate");
        {
            auto dir = tempDir.getChildFile ("edit-user");
            dir.createDirectory();
            savePreset (dir, "kit", { "drums" }, "", format);

            PresetLibrary library (format, dir);
            library.rescan();

            auto original = library.getAll()[0];
            auto newMeta = original.meta;
            newMeta.name = "kit edited";
            newMeta.comment = "now with extra punch";

            PresetEntry copyOut;
            juce::String err;
            expect (library.writeEditableCopy (original, newMeta, copyOut, err), err);

            // The original is untouched -- this was a duplicate-then-edit, not an in-place edit.
            expect (dir.getChildFile ("kit" + format.extension).existsAsFile());
            expectEquals (PresetFile::load (dir.getChildFile ("kit" + format.extension), format)
                              ->meta.comment,
                          original.meta.comment);

            expectEquals (copyOut.file.getFileNameWithoutExtension(), juce::String ("kit edited"));
            expectEquals (PresetFile::load (copyOut.file, format)->meta.comment,
                          juce::String ("now with extra punch"));
        }

        tempDir.deleteRecursively();
    }
};

static PresetLibrarySortAndFilterTests presetLibrarySortAndFilterTests;

struct PresetMigrationTests : juce::UnitTest
{
    PresetMigrationTests() : juce::UnitTest ("atl::PresetFile migration", "presets") {}

    void runTest() override
    {
        const auto format = testFormat();
        auto tempDir = juce::File::createTempFile ("atl-migration-tests");
        tempDir.deleteFile();
        tempDir.createDirectory();

        beginTest ("countPresetFiles counts by extension, recursively");
        {
            auto dir = tempDir.getChildFile ("count");
            dir.createDirectory();
            savePreset (dir, "one", {}, "", format);
            savePreset (dir, "two", {}, "", format);
            auto sub = dir.getChildFile ("sub");
            sub.createDirectory();
            savePreset (sub, "three", {}, "", format);
            dir.getChildFile ("readme.txt").replaceWithText ("hi");

            expectEquals (PresetFile::countPresetFiles (dir, format), 3);
            expectEquals (PresetFile::countPresetFiles (tempDir.getChildFile ("nope"), format), 0);
        }

        beginTest ("moveAllPresets moves preset files, including one in a subfolder, and skips others");
        {
            auto from = tempDir.getChildFile ("move-from");
            from.createDirectory();
            const auto top = savePreset (from, "Kick", {}, "", format);
            auto sub = from.getChildFile ("Drums");
            sub.createDirectory();
            const auto nested = savePreset (sub, "Snare", {}, "", format);
            from.getChildFile ("notes.txt").replaceWithText ("keep me");

            auto to = tempDir.getChildFile ("move-to");

            const auto result = PresetFile::moveAllPresets (from, to, format);
            expect (result.ok, result.error);
            expectEquals ((int) result.moved.size(), 2);
            expectEquals ((int) result.movedFrom.size(), 2);
            expectEquals ((int) result.skipped.size(), 1);
            expectEquals (result.skipped[0], juce::String ("notes.txt"));

            expect (to.getChildFile ("Kick" + format.extension).existsAsFile());
            expect (to.getChildFile ("Drums").getChildFile ("Snare" + format.extension).existsAsFile(),
                    "the subfolder is kept under the destination");
            expect (! top.existsAsFile(), "moved, not copied");
            expect (! nested.existsAsFile());

            // moved/movedFrom line up index-for-index, old path to new path.
            for (int i = 0; i < result.moved.size(); ++i)
            {
                const bool wasTop = result.movedFrom[i].getFullPathName() == top.getFullPathName();
                const bool wasNested = result.movedFrom[i].getFullPathName() == nested.getFullPathName();
                expect (wasTop || wasNested);
                if (wasTop)
                    expectEquals (result.moved[i].getFullPathName(),
                                  to.getChildFile ("Kick" + format.extension).getFullPathName());
                if (wasNested)
                    expectEquals (result.moved[i].getFullPathName(),
                                  to.getChildFile ("Drums")
                                      .getChildFile ("Snare" + format.extension)
                                      .getFullPathName());
            }

            // A .txt stayed behind, on purpose.
            expect (result.sourceHasOtherContent);
            expect (from.getChildFile ("notes.txt").existsAsFile());
        }

        beginTest ("a name already taken in the destination is resolved, not overwritten");
        {
            auto from = tempDir.getChildFile ("collide-from");
            from.createDirectory();
            savePreset (from, "kit", { "new" }, "", format);

            auto to = tempDir.getChildFile ("collide-to");
            to.createDirectory();
            savePreset (to, "kit", { "old" }, "", format);

            const auto result = PresetFile::moveAllPresets (from, to, format);
            expect (result.ok, result.error);
            expectEquals ((int) result.moved.size(), 1);
            expectEquals (result.moved[0].getFileName(), juce::String ("kit 2") + format.extension);

            // The file that was already there is exactly as it was -- not overwritten.
            const auto untouched = PresetFile::load (to.getChildFile ("kit" + format.extension), format);
            expect (untouched.has_value());
            expectEquals (untouched->meta.tags.joinIntoString (","), juce::String ("old"));

            const auto moved = PresetFile::load (result.moved[0], format);
            expect (moved.has_value());
            expectEquals (moved->meta.tags.joinIntoString (","), juce::String ("new"));
        }

        beginTest ("moveAllPresets refuses a destination inside the source");
        {
            auto from = tempDir.getChildFile ("refuse-from");
            from.createDirectory();
            savePreset (from, "kit", {}, "", format);
            auto inside = from.getChildFile ("nested");

            auto result = PresetFile::moveAllPresets (from, inside, format);
            expect (! result.ok);
            expect (result.error.isNotEmpty());
            expect (result.moved.isEmpty());
            expect (result.movedFrom.isEmpty());
            expect (from.getChildFile ("kit" + format.extension).existsAsFile(),
                    "nothing moved when the migration is refused");

            result = PresetFile::moveAllPresets (from, from, format);
            expect (! result.ok, "the same folder is refused too");
        }

        beginTest ("sourceHasOtherContent is false once only empty directories remain");
        {
            auto from = tempDir.getChildFile ("clean-from");
            from.createDirectory();
            savePreset (from, "kit", {}, "", format);
            auto emptySub = from.getChildFile ("empty-sub");
            emptySub.createDirectory();

            auto to = tempDir.getChildFile ("clean-to");
            const auto result = PresetFile::moveAllPresets (from, to, format);
            expect (result.ok, result.error);
            expect (! result.sourceHasOtherContent,
                    "an empty subdirectory left behind doesn't count as content");
        }

        tempDir.deleteRecursively();
    }
};

static PresetMigrationTests presetMigrationTests;

} // namespace
