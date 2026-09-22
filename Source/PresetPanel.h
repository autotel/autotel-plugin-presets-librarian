#pragma once
#include "PresetLibrary.h"
#include "PresetPanelStyle.h"
#include "PresetPanelViews.h"
#include "PresetEditPopover.h"
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

    /** What the plugin wants listed about the live document without loading it -- see
        `PresetMetadata::extra`. Asked for at save time only; the default is nothing, which is the
        right answer for a plugin whose presets need no extra columns. */
    virtual juce::ValueTree captureExtraMetadata() { return {}; }

    /** Where presets are saved and listed. Never empty: a plugin with no folder configured should
        answer with its own default (see `PresetFile::defaultPresetFolder`). */
    virtual juce::File presetFolder() const = 0;
    virtual void setPresetFolder (const juce::File&) = 0;

    /** The document now lives in `file` under this metadata -- after a save, or after renaming the
        file the live document came from. A plugin that shows the patch name elsewhere updates it
        here. */
    virtual void documentSaved (const juce::File& file, const PresetMetadata& meta) { juce::ignoreUnused (file, meta); }

    /** Throw the live document away and start from a blank one -- what the panel's New does.

        The default hands `applyDocument` an invalid tree, which is the right answer for a plugin
        whose loader already falls back to its defaults when handed something that is not one of
        its documents. Override it when starting fresh means more than that. */
    virtual void newDocument() { applyDocument ({}); }

    /** The document now lives in `file` under this metadata -- after a plain Load. Together with
        `documentSaved()` above, this covers every path by which the live document's on-disk
        identity can change; a plugin that shows the patch name elsewhere updates it here too, not
        just in `documentSaved()`, because loading is the one path that changes it without any file
        being written. */
    virtual void documentLoaded (const juce::File& file, const PresetMetadata& meta) { juce::ignoreUnused (file, meta); }

    /** Whether the live document has edits not yet written to its file. Drives the dirty guard
        and the pinned strip's state chip. A host that cannot tell should answer true.

        Defaulting to true means a host that does not implement it gets the load prompt every time
        -- annoying but never destructive, which is the right default. The panel never tries to
        infer dirtiness by comparing trees. It asks a few times a second while it is showing, so
        the answer should be a flag, not a computation; `PresetPanel::refreshLiveState()` makes
        the panel ask right away. */
    virtual bool documentHasUnsavedChanges() const { return true; }

    /** Move every preset file from the current folder to `dest`, then adopt `dest`. Returns what
        moved and what was skipped so the panel can report it.

        The default does exactly that with `PresetFile::moveAllPresets` and, only if every file
        moved, `setPresetFolder (dest)` -- on a failure the old folder stays the live one. Override
        it when a plugin keeps something else in its preset folder that must travel too, or
        remembers the open preset's path itself: the panel follows the open preset to its new
        home, but nothing else tells the plugin that the file moved. */
    virtual MigrationResult migratePresetFolder (const juce::File& dest, const PresetFormat& format)
    {
        auto result = PresetFile::moveAllPresets (presetFolder(), dest, format);
        if (result.ok) setPresetFolder (dest);
        return result;
    }
};

/** Optional: a plugin that can preview a preset without loading it implements this and hands it to
    the panel, which then shows an audition button beside the selected row's `load` and drives it
    from the selection.

    The librarian has no idea this is audio -- it asks whether a row can be previewed and says when
    to start and stop. Everything about *what* is previewed, and where the data for it comes from,
    stays in the plugin. */
struct PresetAuditioner
{
    virtual ~PresetAuditioner() = default;

    /** Whether this row has anything to preview -- drives the button's enablement. */
    virtual bool canAudition (const PresetEntry&) = 0;
    virtual void startAudition (const PresetEntry&) = 0;
    virtual void stopAudition() = 0;
};

/** Optional plugin-supplied pieces of the panel. All of them may be left unset, which is the plain
    librarian; a plugin that wants a waveform beside the fields, or a filter only it understands,
    fills in what it needs rather than the panel growing a field per plugin.

    Components are borrowed, never owned: they must outlive the panel. */
struct PresetPanelExtras
{
    /** Sits inside the edit popover, above `file name`. Shown while the popover is open. */
    juce::Component* detailsHeader = nullptr;
    int detailsHeaderHeight = 0;

    /** Sits in the filter bar's own row, under the search field. */
    juce::Component* extraFilters = nullptr;
    int extraFiltersHeight = 0;

    /** Applied after the panel's own text/tag/source filtering. Call `refilter()` when whatever
        it reads has changed. */
    std::function<bool (const PresetEntry&)> extraFilterPredicate;
};

/** The knobs of the panel's behaviour. The defaults are the design's; a plugin changes one only
    with a reason. */
struct PresetPanelOptions
{
    /** When Load (and New) stop to ask before replacing the live document. */
    enum class LoadGuard
    {
        dirty,    // only when the host says there are unsaved changes
        always,   // every time something would be replaced -- except reloading a clean open preset
        none      // never
    };

    bool showTagRail = true;
    bool showComments = true;   // the comment line under each row
    LoadGuard loadGuard = LoadGuard::dirty;
    bool confirmDelete = true;
};

/** The librarian tab: one column -- folder bar, filter bar, the tag filters in force -- over a tag
    rail and the list, with the live patch pinned at the top of the list and a status bar under it.

    **Filename-driven.** A preset's name is its file's name (see `PresetMetadata`). The fields that
    describe a preset open *at* the preset -- a popover under its row, or under the live strip --
    and applying a changed name renames the file. The annotation a filename cannot carry goes in
    `comment`, which is shown in the list and is never part of a name.

    **Browsing is not loading.** Selecting a row highlights it and grows its action strip; it never
    touches the live document. Load -- the row's button, a double-click, Return -- is the only thing
    that does, and when the live document has something to lose it passes a guard first (see
    `PresetPanelOptions::loadGuard` and `PresetHost::documentHasUnsavedChanges`).

    **Tags are objects.** Every tag is listed with a count in the rail; clicking one there or on a
    row filters by it; dragging a row onto one assigns it; and the popover's picker adds, removes
    and creates them. */
class PresetPanel : public juce::Component,
                    public juce::DragAndDropContainer,
                    private PresetListView::Model,
                    private juce::Timer
{
public:
    PresetPanel (PresetHost&, PresetFormat);
    ~PresetPanel() override;

    /** Re-filters what is already in memory; does **not** touch the disk. Cheap enough to call on
        every editor refresh. A real rescan happens at construction, from the refresh button, and
        after anything this panel writes. */
    void refresh();

    /** Re-reads the folder. For a plugin that writes presets from somewhere other than this panel
        -- a Save button of its own, an autosave -- so the list catches up. */
    void rescan();

    /** The presets the plugin ships inside its binary. See `PresetLibrary::setBuiltIns`. */
    void setBuiltIns (std::vector<BuiltInPreset>);

    /** The selected row, or null when the selection was cleared. For a plugin that shows something
        of its own about the selection -- a waveform, a thumbnail -- in `PresetPanelExtras`. */
    std::function<void (const PresetEntry*)> onSelectionChanged;

    void setColours (PresetPanelColours);
    void setOptions (PresetPanelOptions);
    const PresetPanelOptions& getOptions() const { return options; }

    /** Plugin-supplied pieces -- see `PresetPanelExtras`. Call before the panel is shown. */
    void setExtras (PresetPanelExtras);

    /** Turns on the audition button and drives it from the selected row. Null switches it off.
        Not owned. */
    void setAuditioner (PresetAuditioner*);

    /** Re-applies the filter, including the plugin's own predicate. For a plugin whose extra filter
        controls have just changed; `refresh()` does this too, along with the folder label. */
    void refilter();

    /** Asks the host about unsaved changes now rather than at the next poll -- for a plugin that
        knows the moment its document changed. */
    void refreshLiveState();

    /** Favourites are a per-preset star, kept by the panel rather than written into the preset:
        starring is a preference about a file, not a property of it, and starring a factory preset
        must not need a copy of it. A plugin that wants them to outlast the editor stores what
        `getFavourites()` returns (and `onFavouritesChanged` says when) and hands it back with
        `setFavourites()`. The strings are opaque, but stable across a folder migration. */
    juce::StringArray getFavourites() const { return favourites; }
    void setFavourites (const juce::StringArray&);
    std::function<void()> onFavouritesChanged;

    /** What the status bar says: the last thing that happened. */
    juce::String getStatus() const { return status; }

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void dragOperationEnded (const juce::DragAndDropTarget::SourceDetails&) override;

private:
    // ── PresetListView::Model ────────────────────────────────────────────────
    int getNumRows() override { return (int) rows.size(); }
    const PresetEntry* getRow (int) override;
    bool isRowOpen (const PresetEntry&) override;
    bool isRowFavourite (const PresetEntry&) override;
    bool isTagFiltering (const juce::String&) override;
    bool showComments() override { return options.showComments; }
    bool canAudition (const PresetEntry&) override;
    bool hasAuditioner() override { return auditioner != nullptr; }
    void rowSelected (int) override;
    void rowLoadRequested (int) override;
    void rowEditRequested (int) override;
    void rowDuplicateRequested (int) override;
    void rowDeleteRequested (int) override;
    void rowAuditionRequested (int) override;
    void rowRevealRequested (int) override;
    void rowFavouriteToggled (int) override;
    void rowTagClicked (const juce::String&) override;
    void rowDragStarted (int) override;
    void escapePressed() override;

    void timerCallback() override;

    // ── state ────────────────────────────────────────────────────────────────
    void applyColours();
    void rescanAndRefreshList();
    void applyFilterAndRefreshList();
    void refreshTagViews();
    void refreshLiveStrip();
    void refreshFolderLabel();
    void setStatus (const juce::String&);

    const PresetEntry* findEntry (const juce::String& key) const;
    int rowIndexOf (const juce::String& key) const;
    void selectKey (const juce::String& key);
    juce::String favouriteKey (const PresetEntry&) const;

    /** The open preset's library entry, or null when nothing is open or it has left the folder. */
    const PresetEntry* openEntry() const;
    bool liveHasFile() const { return currentFile != juce::File(); }
    bool isDirty() const { return host.documentHasUnsavedChanges(); }

    // ── loading ──────────────────────────────────────────────────────────────
    /** Load, behind the guard: the only way anything in this panel replaces the live document. */
    void requestLoad (const PresetEntry&);
    /** What the guard would do for this entry, without asking. */
    bool loadNeedsConfirmation (const PresetEntry*) const;
    void loadEntry (const PresetEntry&);
    void doRevert();

    // ── writing ──────────────────────────────────────────────────────────────
    /** Writes the live document to `file` under `meta`, adopts it as the open one and rescans.
        False (with the error shown) on failure. */
    bool writeLiveTo (const juce::File& file, PresetMetadata meta);
    /** The strip's save: back to the open file, or -- for a patch with no file of its own yet -- a
        new one in the preset folder under the live patch's name. Never overwrites a file it was
        not opened from. */
    bool doSave();
    void doSaveAs();
    void doNew();
    void newNow();
    /** The popover's apply for a preset in the list. `entry` is a copy: the library rescans under
        it. */
    void applyToEntry (PresetEntry entry, PresetMetadata draft);
    void deleteNow (PresetEntry);
    void assignTag (const juce::String& tag, const juce::String& key);

    /** What the live patch is called and described as, for a strip, a save or a popover.
        `askDocument` fills a missing name or author from the live document itself. */
    PresetMetadata liveMetadata (bool askDocument = false) const;
    juce::String liveName() const;

    // ── popover ──────────────────────────────────────────────────────────────
    enum class EditTarget { none, row, live, saveAs };
    void openEditorForRow (int row);
    void openEditorForLive();
    void closeEditor();
    void applyEditor();

    // ── folder ───────────────────────────────────────────────────────────────
    void choosePresetFolder();
    void openMigrate();
    void runMigrate (const juce::File& dest);
    void showMigrateDone (const juce::File& oldFolder, const MigrationResult&);

    // ── modal ────────────────────────────────────────────────────────────────
    PresetModal& showModal (int cardWidth);
    /** Lays the modal out now that its parts are in, and gives it the keyboard. */
    void finishModal();
    void closeModal();
    void showError (const juce::String& message);

    void stopAuditionIfRunning();

    PresetHost& host;
    PresetFormat format;
    PresetLibrary library;
    PanelPalette palette;
    PresetPanelLookAndFeel lookAndFeel;
    PresetPanelOptions options;
    PresetPanelExtras extras;
    PresetAuditioner* auditioner = nullptr;

    PresetFilter filter;
    juce::StringArray activeTags;
    PresetSortKey sortKey = PresetSortKey::name;
    bool sortAscending = true;
    juce::StringArray favourites;

    /** Pointers into the library: valid only while `rowsGeneration` matches it. A library
        mutation rescans before the panel refreshes, and anything that lays the list out in
        between -- closing the popover, say -- would otherwise read freed entries. */
    std::vector<const PresetEntry*> rows;
    int rowsGeneration = -1;
    /** The selection by key, so it survives a rescan, a re-sort and a refilter. */
    juce::String selectedKey;

    /** The file the live document was last loaded from or saved to. Empty means it has no on-disk
        identity yet, so Save writes a new file. Panel-local: the plugin is not asked to track a
        "current preset file" for a purely UI need. */
    juce::File currentFile;

    /** `PresetEntry::key()` of the open preset. Tracked beside `currentFile` because a built-in has
        no file, so an empty `currentFile` cannot tell "nothing open" from "a built-in is open". */
    juce::String currentKey;

    /** The metadata the live patch carries when it has no file to read it from: named in the
        popover before its first save, or inherited from the built-in it was loaded from. */
    PresetMetadata pendingLiveMeta;

    EditTarget editTarget = EditTarget::none;
    juce::String editKey;
    bool lastDirty = false;
    juce::String status;
    juce::Rectangle<int> folderBarArea, filterBarArea, statusBarArea, sortArea, sourceArea,
                         extraFiltersArea;

    // ── components ───────────────────────────────────────────────────────────
    juce::TextButton folderButton, migrateButton;
    juce::Label folderLabel;
    std::unique_ptr<juce::FileChooser> folderChooser;

    juce::TextEditor searchBox;
    juce::TextButton builtInToggle { "built-in" }, userToggle { "mine" };
    juce::ComboBox sortBox;
    juce::TextButton sortDirectionButton, refreshButton { "refresh" };

    ActiveFilterBar activeFilterBar;
    TagRail tagRail;
    LiveStrip liveStrip;
    /** Before `list`, which borrows it: the list lets go of it on destruction, so it must still
        exist then. */
    PresetEditPopover popover;
    PresetListView list;

    juce::Label statusLabel;
    juce::TextButton newButton { "new" }, saveAsButton { "save as" };

    std::unique_ptr<PresetModal> modal;
    /** The last modal closed, kept alive until the next one closes: a modal is usually closed from
        one of its own buttons, whose `onClick` is still running when it does. */
    std::unique_ptr<PresetModal> retiredModal;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetPanel)
};

} // namespace atl
