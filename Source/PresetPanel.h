#pragma once
#include "PresetLibrary.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>

namespace atl
{

/** What the panel needs from the plugin it is embedded in, and nothing more: a document to save, a
    document to load into, and where presets live. Implement it once per plugin -- that adapter is
    the only place the librarian and the plugin meet. */
struct PresetHost
{
    virtual ~PresetHost() = default;

    /** The live document, serialised. Called at save time and to read the current name/author. */
    virtual juce::ValueTree captureDocument() = 0;

    /** Load a document that was just read from disk. The plugin is expected to do whatever
        follows -- push to its engine, tell its UI -- before returning. */
    virtual void applyDocument (const juce::ValueTree&) = 0;

    /** Where presets are saved and listed. Never empty: a plugin with no folder configured should
        answer with its own default (see `PresetFile::defaultPresetFolder`). */
    virtual juce::File presetFolder() const = 0;
    virtual void setPresetFolder (const juce::File&) = 0;

    /** The document now lives in `file` under this metadata -- after a save, or after renaming the
        file the live document came from. A plugin that shows the patch name elsewhere updates it
        here. */
    virtual void documentSaved (const juce::File& file, const PresetMetadata& meta) { juce::ignoreUnused (file, meta); }

    /** The document now lives in `file` under this metadata -- after a plain Load. Together with
        `documentSaved()` above, this covers every path by which the live document's on-disk
        identity can change; a plugin that shows the patch name elsewhere updates it here too, not
        just in `documentSaved()`, since selecting a row loads it immediately and never calls
        that. */
    virtual void documentLoaded (const juce::File& file, const PresetMetadata& meta) { juce::ignoreUnused (file, meta); }
};

/** Colours the panel draws with. Defaults are a dark neutral set; a plugin with its own theme
    passes its own so the tab does not look like a guest. */
struct PresetPanelColours
{
    juce::Colour background { 0xff1a1a1c };
    juce::Colour panel      { 0xff232327 };
    juce::Colour text       { 0xffe4e4e8 };
    juce::Colour textDim    { 0xff8a8a92 };
    juce::Colour accent     { 0xff4aa3ff };
    juce::Colour outline    { 0xff3a3a40 };
};

/** The librarian tab: a filtered list of preset files on the left, the selected preset's details on
    the right, and save / save as / rename / delete.

    **Filename-driven.** A preset's name is its file's name (see `PresetMetadata`): the name field
    edits the filename with the extension shown beside it, dimmed; editing it disables Save and
    leaves Save As, because Save means "write back to the file this document came from"; and Save As
    writes straight into the preset folder rather than opening a file chooser, since the folder is
    this panel's own setting and the name is the field above the button -- which is why its label
    carries no ellipsis. The annotation a filename cannot carry goes in the comment field, which is
    shown in the list and is never part of a name.

    Selecting a row loads it immediately -- the librarian is for browsing sounds, and a click that
    only highlights is a click wasted. Nothing is destructive until Save, Rename or Delete. */
class PresetPanel : public juce::Component
{
public:
    PresetPanel (PresetHost&, PresetFormat);
    ~PresetPanel() override;

    /** Re-filters what is already in memory; does **not** touch the disk. Cheap enough to call on
        every editor refresh. A real rescan happens at construction, from the refresh button, and
        after anything this panel writes. */
    void refresh();

    void setColours (PresetPanelColours);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct ListModel : public juce::ListBoxModel
    {
        explicit ListModel (PresetPanel& o) : owner (o) {}
        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
        void selectedRowsChanged (int lastRowSelected) override;
        /** Right-click opens the row's menu; a plain click is left to `selectedRowsChanged`, which
            is what loads the preset. */
        void listBoxItemClicked (int row, const juce::MouseEvent&) override;
        PresetPanel& owner;
    };

    void applyColours();
    void choosePresetFolder();
    void refreshFolderLabel();

    void rescanAndRefreshList();
    void applyFilterAndRefreshList();
    void selectionChanged (int row);
    void updateDetailsFromSelection();
    void updateEnablement();

    void loadEntry (const PresetEntry&);
    PresetMetadata metadataFromEditors() const;

    void doSave();
    void doSaveAs();
    /** Writes the live document to `file`, adopts it as the open one and refreshes. */
    void writeTo (const juce::File& file);
    void doDelete();
    void doRename();
    void doDuplicate();

    /** What is typed in the name field, sanitised into a filename. Never empty. */
    juce::String typedBaseName() const;
    /** Whether the name field still names the file the document is open from. False is what
        disables Save. */
    bool nameMatchesCurrentFile() const;
    void showRowMenu (int row);

    void showError (const juce::String& message);

    PresetHost& host;
    PresetFormat format;
    PresetLibrary library;
    PresetPanelColours colours;

    PresetFilter filter;
    std::vector<const PresetEntry*> rows;
    int selectedRow = -1;
    PresetEntry selectedCopy;
    bool haveSelection = false;

    /** The file the live document was last loaded from or saved to. Empty means it has no on-disk
        identity yet, so Save behaves like Save As. Panel-local: the plugin is not asked to track
        a "current preset file" for a purely UI need. */
    juce::File currentFile;

    // ── list column ──────────────────────────────────────────────────────────
    juce::TextEditor searchBox;
    juce::ComboBox tagFilterBox;
    juce::TextButton refreshButton { "refresh" };
    juce::ListBox list { "presets", nullptr };
    ListModel listModel { *this };
    juce::Label statusLabel;

    juce::TextButton folderButton { "folder..." };
    juce::Label      folderLabel;
    std::unique_ptr<juce::FileChooser> folderChooser;

    // ── details column ───────────────────────────────────────────────────────
    juce::Label nameLabel { {}, "file name" };
    juce::TextEditor nameEditor;
    /** The extension, dimmed: part of the filename, not part of what may be typed. */
    juce::Label extensionLabel;
    juce::Label authorLabel { {}, "author" };
    juce::TextEditor authorEditor;
    juce::Label commentLabel { {}, "comment" };
    juce::TextEditor commentEditor;
    juce::Label descriptionLabel { {}, "description" };
    juce::TextEditor descriptionEditor;
    juce::Label tagsLabel { {}, "tags (comma separated)" };
    juce::TextEditor tagsEditor;
    juce::Label dateLabel;

    juce::TextButton saveButton { "save" };
    juce::TextButton saveAsButton { "save as" };
    juce::TextButton renameButton { "rename / apply" };
    juce::TextButton deleteButton { "delete" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetPanel)
};

} // namespace atl
