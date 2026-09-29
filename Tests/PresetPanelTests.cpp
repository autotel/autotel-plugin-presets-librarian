#include "PresetPanel.h"
#include <juce_core/juce_core.h>

using namespace atl;

namespace
{

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

/** A minimal PresetHost recording which of documentLoaded()/documentSaved() fired and with what,
    so a plugin's "show the name outside the panel" logic has something to assert against. `dirty`
    is what it answers when the panel asks about unsaved changes. */
struct RecordingHost : PresetHost
{
    juce::ValueTree document { "Doc" };

    juce::ValueTree captureDocument() override { return document; }
    void applyDocument (const juce::ValueTree& t) override { document = t; }
    juce::File presetFolder() const override { return folder; }
    void setPresetFolder (const juce::File& f) override { folder = f; }
    bool documentHasUnsavedChanges() const override { return dirty; }
    void newDocument() override { newCount++; document = juce::ValueTree { "Doc" }; }

    void documentSaved (const juce::File& f, const PresetMetadata& m) override
    {
        savedCount++; lastFile = f; lastMeta = m;
        dirty = false;
    }
    void documentLoaded (const juce::File& f, const PresetMetadata& m) override
    {
        loadedCount++; lastFile = f; lastMeta = m;
        dirty = false;
    }

    juce::File folder;
    bool dirty = false;
    int savedCount = 0, loadedCount = 0, newCount = 0;
    juce::File lastFile;
    PresetMetadata lastMeta;
};

/** Depth-first over a component tree, for the first `T` that `pred` accepts. The panel's pieces
    are found the way a person finds them -- by what they are and what they say -- rather than
    through test-only accessors. */
template <typename T>
T* find (juce::Component& root, std::function<bool (T&)> pred = {})
{
    if (auto* t = dynamic_cast<T*> (&root); t != nullptr && (pred == nullptr || pred (*t)))
        return t;
    for (int i = 0; i < root.getNumChildComponents(); ++i)
        if (auto* found = find<T> (*root.getChildComponent (i), pred))
            return found;
    return nullptr;
}

PresetModal* findModal (juce::Component& root)
{
    return find<PresetModal> (root, [] (PresetModal& m) { return m.isVisible() && m.getParentComponent() != nullptr; });
}

juce::MouseEvent mouseAt (juce::Component* c, juce::Point<float> pos, int clicks)
{
    // position, mods, pressure, orientation, rotation, tiltX, tiltY, then the two components,
    // the two times either side of mouseDownPosition, and the click count.
    return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), pos, {}, {}, {}, {},
                             {}, {}, c, c, {}, pos, {}, clicks, false);
}

juce::File freshDir (const juce::String& name)
{
    auto d = juce::File::createTempFile (name);
    d.deleteFile();
    d.createDirectory();
    return d;
}

/** Drives a real PresetPanel the way a person would: select a row, press its buttons, answer its
    dialogs. Everything here is synchronous -- a `TextButton`'s `onClick` is a public
    `std::function`, the list's selection calls its model directly, and the panel's dialogs are
    components inside it rather than native windows -- so no message loop is needed. */
struct PresetPanelTests : juce::UnitTest
{
    PresetPanelTests() : juce::UnitTest ("atl::PresetPanel", "presets") {}

    void runTest() override
    {
        const auto format = testFormat();
        auto tempDir = freshDir ("atl-presetpanel-tests");

        PresetFile::save (tempDir.getChildFile ("kit" + format.extension), makeDocument(),
                          makeMeta(), format);

        RecordingHost host;
        host.folder = tempDir;

        PresetPanel panel (host, format);
        panel.setSize (1000, 700);

        auto* list = find<PresetListView> (panel);
        auto* strip = find<LiveStrip> (panel);

        beginTest ("the panel is built from one list and one live strip");
        {
            expect (list != nullptr, "PresetPanel must own a PresetListView");
            expect (strip != nullptr, "PresetPanel must own a LiveStrip");
            if (list == nullptr || strip == nullptr) return;
        }

        beginTest ("selecting a row does not load it");
        {
            list->selectRow (0);

            expectEquals (host.loadedCount, 0);
            expectEquals (host.savedCount, 0);
            expect (list->loadButton.isVisible(), "a selected row grows its action strip");
        }

        beginTest ("the load button fires documentLoaded, not documentSaved");
        {
            list->loadButton.onClick();

            expectEquals (host.loadedCount, 1);
            expectEquals (host.savedCount, 0);
            expectEquals (host.lastFile.getFileNameWithoutExtension(), juce::String ("kit"));
            expectEquals (host.lastMeta.name, juce::String ("kit"));
            expectEquals (strip->getState().name, juce::String ("kit"));
            expect (findModal (panel) == nullptr, "a clean live patch loads without asking");
        }

        beginTest ("Return on the list is a shortcut for Load");
        {
            list->keyPressed (juce::KeyPress (juce::KeyPress::returnKey));
            expectEquals (host.loadedCount, 2);
            expectEquals (host.savedCount, 0);
        }

        beginTest ("double-clicking a row is a shortcut for Load");
        {
            auto* content = find<juce::Viewport> (*list)->getViewedComponent();
            // Right of the tags, left of the date: plain row, not a star or a chip.
            content->mouseDoubleClick (mouseAt (content, { 500.0f, 10.0f }, 2));
            expectEquals (host.loadedCount, 3);
        }

        beginTest ("a double-click on a row that is not the selected one loads nothing");
        {
            auto* content = find<juce::Viewport> (*list)->getViewedComponent();
            content->mouseDoubleClick (mouseAt (content, { 200.0f, 600.0f }, 2));   // below every row
            expectEquals (host.loadedCount, 3);
        }

        beginTest ("loading over unsaved changes asks first, and cancel loads nothing");
        {
            host.dirty = true;
            panel.refreshLiveState();
            expect (strip->getState().dirty);

            list->loadButton.onClick();
            auto* modal = findModal (panel);
            expect (modal != nullptr, "a dirty live patch must not be replaced without asking");
            expectEquals (host.loadedCount, 3);

            if (modal != nullptr)
            {
                expect (modal->findButton ("save, then load") != nullptr);
                expect (modal->findButton ("load anyway") != nullptr);
                modal->findButton ("cancel")->onClick();
            }
            expect (findModal (panel) == nullptr);
            expectEquals (host.loadedCount, 3);
        }

        beginTest ("load anyway loads");
        {
            list->loadButton.onClick();
            if (auto* modal = findModal (panel))
                modal->findButton ("load anyway")->onClick();
            expectEquals (host.loadedCount, 4);
            expectEquals (host.savedCount, 0);
        }

        beginTest ("save, then load writes the open preset back before replacing it");
        {
            host.dirty = true;
            list->loadButton.onClick();
            if (auto* modal = findModal (panel))
                modal->findButton ("save, then load")->onClick();
            expectEquals (host.savedCount, 1);
            expectEquals (host.loadedCount, 5);
            expectEquals (host.lastFile.getFileNameWithoutExtension(), juce::String ("kit"));
        }

        beginTest ("the load guard is one knob");
        {
            auto options = panel.getOptions();

            options.loadGuard = PresetPanelOptions::LoadGuard::none;
            panel.setOptions (options);
            host.dirty = true;
            list->selectRow (0);
            list->loadButton.onClick();
            expect (findModal (panel) == nullptr, "none never asks");
            expectEquals (host.loadedCount, 6);

            options.loadGuard = PresetPanelOptions::LoadGuard::always;
            panel.setOptions (options);
            list->selectRow (0);
            list->loadButton.onClick();
            expect (findModal (panel) == nullptr, "reloading the clean open preset replaces nothing");
            expectEquals (host.loadedCount, 7);

            options.loadGuard = PresetPanelOptions::LoadGuard::dirty;
            panel.setOptions (options);
        }

        beginTest ("the live strip saves only what there is to save");
        {
            host.dirty = false;
            panel.refreshLiveState();
            expect (! strip->saveButton.isEnabled(), "a clean patch with a file has nothing to save");
            expect (! strip->revertButton.isEnabled());

            host.dirty = true;
            panel.refreshLiveState();
            expect (strip->saveButton.isEnabled());
            expect (strip->revertButton.isEnabled());

            strip->revertButton.onClick();
            expectEquals (host.loadedCount, 8);
            expect (panel.getStatus().startsWith ("reverted"));
        }

        beginTest ("a new, never-saved patch can be named from the strip and saved");
        {
            host.dirty = false;
            find<juce::TextButton> (panel, [] (juce::TextButton& b) { return b.getButtonText() == "new"; })->onClick();
            expectEquals (host.newCount, 1);
            expect (strip->getState().name.isEmpty(), "a new patch has no name until it is given one");
            expect (strip->saveButton.isEnabled(), "a new patch can always be saved -- that is how it gets a file");
            expect (strip->editButton.isEnabled());

            strip->editButton.onClick();
            auto* popover = find<PresetEditPopover> (panel);
            expect (popover != nullptr && popover->isVisible());
            if (popover == nullptr) return;
            expect (popover->getMode() == PresetEditPopover::Mode::liveUnsaved);

            popover->nameEditor.setText ("fresh", false);
            popover->tagPicker.addTag ("warm");
            popover->applyButton.onClick();
            expect (! popover->isVisible());
            expectEquals (strip->getState().name, juce::String ("fresh"));

            const int saves = host.savedCount;
            strip->saveButton.onClick();
            expectEquals (host.savedCount, saves + 1);
            const auto written = tempDir.getChildFile ("fresh" + format.extension);
            expect (written.existsAsFile());
            if (auto loaded = PresetFile::load (written, format))
                expect (loaded->meta.tags.contains ("warm"));
            expectEquals (strip->getState().name, juce::String ("fresh"));
        }

        beginTest ("a new patch is signed like the last preset saved, and never re-signs a file");
        {
            auto dir = freshDir ("atl-presetpanel-author");
            RecordingHost h;
            h.folder = dir;
            PresetPanel p (h, format);
            p.setSize (1000, 700);
            auto* s = find<LiveStrip> (p);
            auto newButton = [&p] { return find<juce::TextButton> (p, [] (juce::TextButton& b) { return b.getButtonText() == "new"; }); };

            auto older = makeMeta();
            older.author = "old hand";
            older.date = "2020-01-01T00:00:00.000Z";
            PresetFile::save (dir.getChildFile ("older" + format.extension), makeDocument(), older, format);
            auto untouched = makeMeta();
            untouched.author = {};
            PresetFile::save (dir.getChildFile ("unsigned" + format.extension), makeDocument(), untouched, format);
            p.rescan();
            newButton()->onClick();

            // Named from the popover, which already shows who it will be signed by.
            s->editButton.onClick();
            auto* popover = find<PresetEditPopover> (p);
            if (popover == nullptr) { expect (false); return; }
            expectEquals (popover->authorEditor.getText(), juce::String ("old hand"));
            popover->authorEditor.setText ("x", false);
            popover->nameEditor.setText ("first", false);
            popover->applyButton.onClick();
            s->saveButton.onClick();

            newButton()->onClick();
            s->saveButton.onClick();   // no name, no author: both come from elsewhere
            expectEquals (h.lastMeta.author, juce::String ("x"));

            // A document that names its own author keeps it.
            newButton()->onClick();
            h.document.setProperty ("author", "doc author", nullptr);
            s->saveButton.onClick();
            expectEquals (h.lastMeta.author, juce::String ("doc author"));

            // Saving an existing file with no author leaves it unsigned.
            h.document = juce::ValueTree { "Doc" };
            const auto unsignedFile = dir.getChildFile ("unsigned" + format.extension);
            auto* l = find<PresetListView> (p);
            auto* search = find<juce::TextEditor> (p, [] (juce::TextEditor& e) { return e.getName() == "preset filter"; });
            search->setText ("unsigned", false);
            search->onTextChange();
            l->selectRow (0);
            l->loadButton.onClick();
            expect (h.lastFile == unsignedFile);
            h.dirty = true;
            s->saveButton.onClick();
            expect (h.lastFile == unsignedFile);
            if (auto meta = PresetFile::scanMetadataOnly (unsignedFile, format))
                expect (meta->author.isEmpty(), "an existing file is never re-signed");
            dir.deleteRecursively();
        }

        beginTest ("edit on a row opens the popover under it, and apply renames the file");
        {
            panel.rescan();
            // Rows are sorted by name: "fresh", then "kit".
            list->selectRow (1);
            const int kitRow = list->getSelectedRow();
            list->editButton.onClick();

            auto* popover = find<PresetEditPopover> (panel);
            expect (popover != nullptr && popover->isVisible());
            expectEquals (list->getPopoverRow(), kitRow);
            if (popover == nullptr) return;
            expectEquals (popover->nameEditor.getText(), juce::String ("kit"));
            expect (popover->getMode() == PresetEditPopover::Mode::edit);

            popover->nameEditor.setText ("kit renamed", false);
            popover->tagPicker.removeTag ("punchy");
            popover->tagPicker.addTag ("Snappy");
            popover->applyButton.onClick();

            expect (! tempDir.getChildFile ("kit" + format.extension).existsAsFile());
            const auto renamed = tempDir.getChildFile ("kit renamed" + format.extension);
            expect (renamed.existsAsFile());
            if (auto meta = PresetFile::scanMetadataOnly (renamed, format))
                expectEquals (meta->tags.joinIntoString (","), juce::String ("drums,Snappy"));
            expect (panel.getStatus().contains ("renamed"));
        }

        beginTest ("the tag picker joins an existing spelling rather than starting a second one");
        {
            TagPicker picker;
            picker.setTags ({ "pad" }, { "Bell", "pad" });
            picker.addTag ("bell");
            picker.addTag ("PAD");
            picker.addTag ("a, b");
            expectEquals (picker.getTags().joinIntoString ("|"), juce::String ("pad|Bell|a|b"));
        }

        beginTest ("a comma in the tag input commits what was typed before it");
        {
            TagPicker picker;
            picker.setTags ({}, { "Bell" });
            picker.input.setText ("bell, dark,, wa", false);
            picker.input.onTextChange();
            expectEquals (picker.getTags().joinIntoString ("|"), juce::String ("Bell|dark"));
            expectEquals (picker.input.getText(), juce::String ("wa"));

            picker.input.onReturnKey();
            expectEquals (picker.getTags().joinIntoString ("|"), juce::String ("Bell|dark|wa"));
            expect (picker.input.isEmpty());
        }

        beginTest ("dropping a preset on a tag assigns it, once");
        {
            auto* rail = find<TagRail> (panel);
            expect (rail != nullptr);
            if (rail == nullptr) return;

            const auto key = tempDir.getChildFile ("fresh" + format.extension).getFullPathName();
            rail->onPresetDropped ("drums", key);
            expect (panel.getStatus().startsWith ("tagged"));
            if (auto meta = PresetFile::scanMetadataOnly (juce::File (key), format))
                expect (meta->tags.contains ("drums"));

            rail->onPresetDropped ("drums", key);
            expect (panel.getStatus().contains ("already tagged"));

            rail->onPresetDropped ("DRUMS", key);
            expect (panel.getStatus().contains ("already tagged"));
        }

        beginTest ("clicking a tag filters by it; match all and match any combine them");
        {
            auto* rail = find<TagRail> (panel);
            auto* bar = find<ActiveFilterBar> (panel);
            if (rail == nullptr || bar == nullptr) { expect (false); return; }
            expect (! bar->isVisible(), "the active filter bar is only there while tags filter");

            rail->onTagClicked ("warm");
            expect (bar->isVisible());
            // "fresh" carries warm and drums; "kit renamed" carries drums and Snappy.
            expectEquals (list->getNumRows(), 1);

            rail->onTagClicked ("Snappy");
            expectEquals (list->getNumRows(), 0, "no preset carries both");

            bar->modeButton.onClick();
            expectEquals (bar->modeButton.getButtonText(), juce::String ("match any"));
            expectEquals (list->getNumRows(), 2);

            bar->clearButton.onClick();
            expect (! bar->isVisible());
        }

        beginTest ("the star is a favourite, and the panel hands it to the plugin to keep");
        {
            int notified = 0;
            panel.onFavouritesChanged = [&notified] { ++notified; };
            list->selectRow (-1, false);
            auto* content = find<juce::Viewport> (*list)->getViewedComponent();
            content->mouseDown (mouseAt (content, { 19.0f, 17.0f }, 1));
            expectEquals (panel.getFavourites().size(), 1);
            expectEquals (notified, 1);
            expectEquals (host.loadedCount, 8, "starring loads nothing");
            content->mouseDown (mouseAt (content, { 19.0f, 17.0f }, 1));
            expectEquals (panel.getFavourites().size(), 0);
            panel.onFavouritesChanged = nullptr;
        }

        beginTest ("delete asks, then moves the file to the trash");
        {
            const auto doomed = tempDir.getChildFile ("fresh" + format.extension);
            panel.rescan();
            list->selectRow (0);   // "fresh"
            list->deleteButton.onClick();
            auto* modal = findModal (panel);
            expect (modal != nullptr);
            expect (doomed.existsAsFile(), "nothing is deleted before the answer");
            if (modal != nullptr) modal->findButton ("delete")->onClick();
            expect (! doomed.existsAsFile());
            expect (panel.getStatus().startsWith ("deleted"));
        }

        beginTest ("migrate moves the presets, adopts the new folder, and keeps the old one unless asked");
        {
            auto dest = freshDir ("atl-presetpanel-migrated");
            dest.deleteRecursively();   // it does not have to exist yet

            find<juce::TextButton> (panel, [] (juce::TextButton& b)
                { return b.getButtonText().startsWith ("migrate"); })->onClick();
            auto* modal = findModal (panel);
            expect (modal != nullptr);
            if (modal == nullptr) return;

            auto* move = modal->findButton ("move presets");
            expect (move != nullptr && ! move->isEnabled(), "nothing moves before a destination is chosen");

            auto* destEditor = find<juce::TextEditor> (*modal);
            // A TextEditor posts its change notification; typing is simulated by calling it.
            destEditor->setText (tempDir.getFullPathName(), false);
            destEditor->onTextChange();
            expect (! move->isEnabled(), "the source is not a destination");
            destEditor->setText (dest.getFullPathName(), false);
            destEditor->onTextChange();
            expect (move->isEnabled());

            move->onClick();
            expectEquals (host.folder.getFullPathName(), dest.getFullPathName());
            expect (dest.getChildFile ("kit renamed" + format.extension).existsAsFile());
            expect (tempDir.isDirectory(), "migrating never deletes the old folder by itself");

            auto* done = findModal (panel);
            expect (done != nullptr);
            if (done != nullptr)
            {
                expect (done->findButton ("delete old folder") != nullptr);
                done->findButton ("keep it")->onClick();
            }
            expect (tempDir.isDirectory());
            dest.deleteRecursively();
        }

        tempDir.deleteRecursively();
    }
};

static PresetPanelTests presetPanelTests;

/** The status bar's, not the popover's apply -- which says "save as" too while it is saving as. */
juce::TextButton* saveAsButton (PresetPanel& panel)
{
    return find<juce::TextButton> (panel, [&panel] (juce::TextButton& b)
        { return b.getButtonText() == "save as" && b.getParentComponent() == &panel; });
}

/** The keyboard, and save as: the paths that pick a name or a row without a mouse. */
struct PresetPanelSaveAsTests : juce::UnitTest
{
    PresetPanelSaveAsTests() : juce::UnitTest ("atl::PresetPanel save as", "presets") {}

    void runTest() override
    {
        const auto format = testFormat();
        auto tempDir = freshDir ("atl-presetpanel-saveas");
        for (auto name : { "a", "b" })
            PresetFile::save (tempDir.getChildFile (juce::String (name) + format.extension), makeDocument(),
                              makeMeta(), format);

        RecordingHost host;
        host.folder = tempDir;
        PresetPanel panel (host, format);
        panel.setSize (1000, 700);
        auto* list = find<PresetListView> (panel);
        if (list == nullptr) { expect (false); return; }

        beginTest ("arrow keys move the selection and load nothing");
        {
            list->keyPressed (juce::KeyPress (juce::KeyPress::downKey));
            expectEquals (list->getSelectedRow(), 0);
            list->keyPressed (juce::KeyPress (juce::KeyPress::downKey));
            expectEquals (list->getSelectedRow(), 1);
            list->keyPressed (juce::KeyPress (juce::KeyPress::downKey));
            expectEquals (list->getSelectedRow(), 1, "the last row stays selected");
            expectEquals (host.loadedCount, 0);
        }

        beginTest ("F2 opens the popover on the selection; Escape closes it");
        {
            list->keyPressed (juce::KeyPress (juce::KeyPress::F2Key));
            expectEquals (list->getPopoverRow(), 1);
            list->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
            expect (! list->isPopoverShowing());
        }

        beginTest ("save as suggests a free name and writes a new file");
        {
            list->selectRow (0);
            list->loadButton.onClick();   // "a" is open

            saveAsButton (panel)->onClick();
            auto* popover = find<PresetEditPopover> (panel);
            if (popover == nullptr) { expect (false); return; }
            expect (popover->getMode() == PresetEditPopover::Mode::saveAs);
            expectEquals (popover->nameEditor.getText(), juce::String ("a 2"));

            popover->applyButton.onClick();
            expect (tempDir.getChildFile ("a 2" + format.extension).existsAsFile());
            expectEquals (host.lastFile.getFileNameWithoutExtension(), juce::String ("a 2"));
        }

        beginTest ("save as over an existing file asks first, and Return is not destruction");
        {
            saveAsButton (panel)->onClick();
            auto* popover = find<PresetEditPopover> (panel);
            popover->nameEditor.setText ("b", false);
            const int saves = host.savedCount;
            popover->applyButton.onClick();

            auto* modal = findModal (panel);
            expect (modal != nullptr, "replacing a file is asked about");
            expectEquals (host.savedCount, saves);
            if (modal == nullptr) return;

            // The only emphasised-looking button in a replace dialog would be the destructive one,
            // so there is none, and Return does nothing.
            modal->keyPressed (juce::KeyPress (juce::KeyPress::returnKey));
            expectEquals (host.savedCount, saves);

            modal->findButton ("replace")->onClick();
            expectEquals (host.savedCount, saves + 1);
            expectEquals (host.lastFile.getFileNameWithoutExtension(), juce::String ("b"));
        }

        tempDir.deleteRecursively();
    }
};

static PresetPanelSaveAsTests presetPanelSaveAsTests;

/** Built-ins in the panel: listed, loadable, never written to -- editing one writes a copy. */
struct PresetPanelBuiltInTests : juce::UnitTest
{
    PresetPanelBuiltInTests() : juce::UnitTest ("atl::PresetPanel built-ins", "presets") {}

    void runTest() override
    {
        const auto format = testFormat();
        auto tempDir = freshDir ("atl-presetpanel-builtins");

        juce::MemoryBlock bellData;
        {
            const auto staging = tempDir.getChildFile ("staging" + format.extension);
            PresetFile::save (staging, makeDocument(), makeMeta(), format);
            staging.loadFileAsData (bellData);
            staging.deleteFile();
        }

        RecordingHost host;
        host.folder = tempDir;
        PresetPanel panel (host, format);
        panel.setSize (1000, 700);
        panel.setBuiltIns ({ { "Bell", "Factory", bellData.getData(), (int) bellData.getSize() } });
        auto* list = find<PresetListView> (panel);
        auto* strip = find<LiveStrip> (panel);
        if (list == nullptr || strip == nullptr) { expect (false); return; }

        beginTest ("a built-in cannot be deleted");
        {
            list->selectRow (0);
            expect (! list->deleteButton.isEnabled());
        }

        beginTest ("apply on a built-in writes an editable copy rather than failing");
        {
            list->editButton.onClick();
            auto* popover = find<PresetEditPopover> (panel);
            if (popover == nullptr) { expect (false); return; }
            expect (popover->getMode() == PresetEditPopover::Mode::factoryCopy);
            popover->nameEditor.setText ("My Bell", false);
            popover->applyButton.onClick();

            const auto copy = tempDir.getChildFile ("My Bell" + format.extension);
            expect (copy.existsAsFile());
            expect (panel.getStatus().contains ("copy"));
        }

        beginTest ("saving a patch loaded from a built-in writes a new file under its name");
        {
            // Rows by name: "Bell", then "My Bell".
            list->selectRow (0);
            list->loadButton.onClick();
            expectEquals (host.loadedCount, 1);
            expect (strip->saveButton.isEnabled(), "no file of its own, so save is how it gets one");
            strip->saveButton.onClick();
            expect (tempDir.getChildFile ("Bell" + format.extension).existsAsFile());
            expectEquals (host.lastFile.getFileNameWithoutExtension(), juce::String ("Bell"));
        }

        tempDir.deleteRecursively();
    }
};

static PresetPanelBuiltInTests presetPanelBuiltInTests;

} // namespace
