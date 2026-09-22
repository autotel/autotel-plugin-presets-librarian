#include "PresetPanel.h"

namespace atl
{

namespace
{
const juce::String ellipsis (juce::CharPointer_UTF8 ("\xe2\x80\xa6"));
const juce::String middot   (juce::CharPointer_UTF8 ("\xc2\xb7"));
const juce::String emDash   (juce::CharPointer_UTF8 ("\xe2\x80\x94"));

juce::String quoted (const juce::String& s)
{
    return juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x9c")) + s
         + juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x9d"));
}

juce::String plural (int n, const juce::String& noun)
{
    return juce::String (n) + " " + noun + (n == 1 ? "" : "s");
}

/** Sort box item ids, in the order the design lists them. */
enum SortId { byName = 1, byDate, byAuthor, byTag, byFavourite, bySource };

PresetSortKey sortKeyFor (int id)
{
    switch (id)
    {
        case byDate:      return PresetSortKey::dateModified;
        case byAuthor:    return PresetSortKey::author;
        case byTag:       return PresetSortKey::firstTag;
        case byFavourite: return PresetSortKey::favouritesFirst;
        case bySource:    return PresetSortKey::source;
        default:          return PresetSortKey::name;
    }
}
} // namespace

// ── construction ────────────────────────────────────────────────────────────

PresetPanel::PresetPanel (PresetHost& h, PresetFormat f)
    : host (h), format (std::move (f)), library (format), palette (PanelPalette::from ({})),
      popover (format.extension)
{
    setLookAndFeel (&lookAndFeel);
    setWantsKeyboardFocus (false);

    // ── folder bar ──
    folderButton.setButtonText ("folder" + ellipsis);
    folderButton.setTooltip ("point the library at a different folder -- nothing is moved");
    folderButton.onClick = [this] { choosePresetFolder(); };
    migrateButton.setButtonText ("migrate" + ellipsis);
    migrateButton.setTooltip ("move your presets to a new folder, then point the library at it");
    migrateButton.onClick = [this] { openMigrate(); };
    for (auto* b : { &folderButton, &migrateButton })
    {
        setButtonStyle (*b, ButtonStyle::plain);
        addAndMakeVisible (*b);
    }
    folderLabel.setName ("preset folder");
    folderLabel.setMinimumHorizontalScale (1.0f);
    addAndMakeVisible (folderLabel);

    // ── filter bar ──
    searchBox.setName ("preset filter");
    searchBox.setTextToShowWhenEmpty ("filter name, author, comment" + ellipsis, palette.textFaint);
    searchBox.onTextChange = [this] { applyFilterAndRefreshList(); };
    searchBox.onEscapeKey = [this] { searchBox.clear(); applyFilterAndRefreshList(); list.grabKeyboardFocus(); };
    searchBox.onReturnKey = [this] { list.grabKeyboardFocus(); };
    addAndMakeVisible (searchBox);

    for (auto* t : { &builtInToggle, &userToggle })
    {
        setButtonStyle (*t, ButtonStyle::segment);
        t->setClickingTogglesState (true);
        t->setToggleState (true, juce::dontSendNotification);
        t->onClick = [this] { applyFilterAndRefreshList(); };
        // Made visible only once a scan has found built-ins -- see applyFilterAndRefreshList.
        addChildComponent (*t);
    }
    builtInToggle.setTooltip ("show the presets that ship with the plugin");
    userToggle.setTooltip ("show the presets in your folder");

    sortBox.setName ("preset sort");
    sortBox.addItem ("name", byName);
    sortBox.addItem ("date modified", byDate);
    sortBox.addItem ("author", byAuthor);
    sortBox.addItem ("first tag", byTag);
    sortBox.addItem ("favourites first", byFavourite);
    sortBox.addItem ("factory / mine", bySource);
    sortBox.setSelectedId (byName, juce::dontSendNotification);
    sortBox.setTooltip ("sort the list");
    sortBox.onChange = [this]
    {
        const int id = sortBox.getSelectedId();
        sortKey = sortKeyFor (id);
        // Newest first and favourites on top are what those two sorts are for.
        sortAscending = ! (id == byDate || id == byFavourite);
        applyFilterAndRefreshList();
    };
    addAndMakeVisible (sortBox);

    setButtonStyle (sortDirectionButton, ButtonStyle::quiet);
    sortDirectionButton.onClick = [this]
    {
        sortAscending = ! sortAscending;
        applyFilterAndRefreshList();
    };
    addAndMakeVisible (sortDirectionButton);

    setButtonStyle (refreshButton, ButtonStyle::plain);
    refreshButton.setTooltip ("re-read the preset folder");
    refreshButton.onClick = [this]
    {
        rescanAndRefreshList();
        setStatus ("rescanned " + middot + " " + plural ((int) library.getAll().size(), "preset"));
    };
    addAndMakeVisible (refreshButton);

    // ── tags ──
    activeFilterBar.onRemoveTag = [this] (const juce::String& t) { rowTagClicked (t); };
    activeFilterBar.onToggleMode = [this]
    {
        filter.matchAnyTag = ! filter.matchAnyTag;
        refreshTagViews();
        applyFilterAndRefreshList();
    };
    activeFilterBar.onClear = [this]
    {
        activeTags.clear();
        refreshTagViews();
        applyFilterAndRefreshList();
    };
    addChildComponent (activeFilterBar);

    tagRail.onTagClicked = [this] (const juce::String& t) { rowTagClicked (t); };
    tagRail.onPresetDropped = [this] (const juce::String& tag, const juce::String& key) { assignTag (tag, key); };
    addAndMakeVisible (tagRail);

    // ── live strip, list, popover ──
    liveStrip.onEdit = [this] { openEditorForLive(); };
    liveStrip.onSave = [this] { doSave(); };
    liveStrip.onRevert = [this] { doRevert(); };
    addAndMakeVisible (liveStrip);

    list.setModel (this);
    list.setPopover (&popover, [this] (int w) { return popover.getHeightForWidth (w); });
    addAndMakeVisible (list);

    popover.onApply = [this] { applyEditor(); };
    popover.onCancel = [this] { closeEditor(); };
    popover.onLayoutChanged = [this] { list.relayout(); };

    // ── status bar ──
    statusLabel.setName ("preset status");
    statusLabel.setMinimumHorizontalScale (1.0f);
    addAndMakeVisible (statusLabel);
    newButton.setTooltip ("start from an empty patch: nothing is open afterwards, so save writes a "
                          "new file");
    newButton.onClick = [this] { doNew(); };
    saveAsButton.setTooltip ("write the live patch to a new file in your folder");
    saveAsButton.onClick = [this] { doSaveAs(); };
    for (auto* b : { &newButton, &saveAsButton })
    {
        setButtonStyle (*b, ButtonStyle::plain);
        addAndMakeVisible (*b);
    }

    applyColours();
    rescanAndRefreshList();
    refreshFolderLabel();
    setStatus (plural ((int) library.getAll().size(), "preset"));

    // The host's dirty flag has no change notification of its own; asking is a virtual call.
    startTimer (300);
}

PresetPanel::~PresetPanel()
{
    stopTimer();
    stopAuditionIfRunning();
    modal.reset();
    retiredModal.reset();
    popover.setHeader (nullptr, 0);
    if (extras.extraFilters != nullptr) removeChildComponent (extras.extraFilters);
    setLookAndFeel (nullptr);
}

// ── public ──────────────────────────────────────────────────────────────────

void PresetPanel::refresh()
{
    applyFilterAndRefreshList();
    refreshFolderLabel();   // the folder can be changed from here, but it is also a plugin setting
}

void PresetPanel::rescan() { rescanAndRefreshList(); }
void PresetPanel::refilter() { applyFilterAndRefreshList(); }
void PresetPanel::refreshLiveState() { refreshLiveStrip(); }

void PresetPanel::setBuiltIns (std::vector<BuiltInPreset> builtIns)
{
    library.setBuiltIns (std::move (builtIns));
    rescanAndRefreshList();
}

void PresetPanel::setColours (PresetPanelColours c)
{
    palette = PanelPalette::from (c);
    applyColours();
    repaint();
}

void PresetPanel::setOptions (PresetPanelOptions o)
{
    options = o;
    tagRail.setVisible (options.showTagRail);
    resized();
    applyFilterAndRefreshList();
}

void PresetPanel::setExtras (PresetPanelExtras e)
{
    if (extras.extraFilters != nullptr) removeChildComponent (extras.extraFilters);
    extras = std::move (e);
    if (extras.extraFilters != nullptr) addAndMakeVisible (*extras.extraFilters);

    // The header lives in the popover, and only while it is open for a row -- see openEditorForRow.
    if (editTarget != EditTarget::none)
        popover.setHeader (extras.detailsHeader, extras.detailsHeaderHeight);

    resized();
    applyFilterAndRefreshList();
}

void PresetPanel::setAuditioner (PresetAuditioner* a)
{
    if (auditioner != nullptr && auditioner != a)
        auditioner->stopAudition();

    auditioner = a;
    list.relayout();
}

void PresetPanel::setFavourites (const juce::StringArray& f)
{
    favourites = f;
    applyFilterAndRefreshList();
}

// ── colours and layout ──────────────────────────────────────────────────────

void PresetPanel::applyColours()
{
    lookAndFeel.setPalette (palette);

    styleEditor (searchBox, palette, 12.5f);
    searchBox.setTextToShowWhenEmpty ("filter name, author, comment" + ellipsis, palette.textFaint);

    folderLabel.setFont (PanelFonts::mono (11.5f));
    folderLabel.setColour (juce::Label::textColourId, palette.base.textDim);
    statusLabel.setFont (PanelFonts::mono (11.0f));
    statusLabel.setColour (juce::Label::textColourId, palette.base.textDim);

    for (auto* c : std::initializer_list<juce::Component*> { &tagRail, &activeFilterBar, &liveStrip, &list, &popover })
        c->sendLookAndFeelChange();
    tagRail.setPalette (palette);
    activeFilterBar.setPalette (palette);
    liveStrip.setPalette (palette);
    list.setPalette (palette);
    popover.setPalette (palette);
    if (modal != nullptr) modal->repaint();
    sendLookAndFeelChange();
}

void PresetPanel::paint (juce::Graphics& g)
{
    g.fillAll (palette.base.background);

    g.setColour (palette.bar);
    g.fillRect (folderBarArea);
    g.fillRect (statusBarArea);
    g.setColour (palette.divider);
    g.fillRect (folderBarArea.getX(), folderBarArea.getBottom() - 1, folderBarArea.getWidth(), 1);
    g.fillRect (filterBarArea.getX(), filterBarArea.getBottom() - 1, filterBarArea.getWidth(), 1);
    if (! extraFiltersArea.isEmpty())
        g.fillRect (extraFiltersArea.getX(), extraFiltersArea.getBottom() - 1, extraFiltersArea.getWidth(), 1);
    g.fillRect (statusBarArea.getX(), statusBarArea.getY(), statusBarArea.getWidth(), 1);

    {
        const auto f = PanelFonts::caps (11.0f);
        g.setFont (f);
        g.setColour (palette.base.textDim);
        g.drawText ("PRESETS", folderBarArea.withTrimmedLeft (12).withTrimmedBottom (1),
                    juce::Justification::centredLeft, false);
    }

    // The sort box and its direction toggle read as one control; so do the two source toggles.
    auto outline = [&g, this] (juce::Rectangle<int> r, bool fill)
    {
        if (r.isEmpty()) return;
        if (fill)
        {
            g.setColour (palette.button);
            g.fillRoundedRectangle (r.toFloat(), 3.0f);
        }
        g.setColour (palette.base.outline);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 3.0f, 1.0f);
    };
    outline (sortArea, true);
    g.setColour (palette.base.outline);
    g.fillRect (sortDirectionButton.getX(), sortArea.getY(), 1, sortArea.getHeight());
    if (builtInToggle.isVisible())
    {
        outline (sourceArea, false);
        g.setColour (palette.base.outline);
        g.fillRect (userToggle.getX(), sourceArea.getY() + 1, 1, sourceArea.getHeight() - 2);
    }
}

void PresetPanel::resized()
{
    auto b = getLocalBounds();
    const auto buttonFont = PanelFonts::sans (12.0f);
    auto widthOf = [&buttonFont] (juce::Button& btn, int pad)
    {
        return (int) std::ceil (PanelFonts::width (buttonFont, btn.getButtonText())) + pad;
    };

    // ── folder bar ──
    folderBarArea = b.removeFromTop (42);
    {
        auto r = folderBarArea.withTrimmedBottom (1).reduced (12, 8);
        migrateButton.setBounds (r.removeFromRight (widthOf (migrateButton, 22)));
        r.removeFromRight (8);
        folderButton.setBounds (r.removeFromRight (widthOf (folderButton, 22)));
        r.removeFromRight (10);
        r.removeFromLeft ((int) std::ceil (PanelFonts::width (PanelFonts::caps (11.0f), "PRESETS")) + 12);
        folderLabel.setBounds (r);
    }

    // ── filter bar ──
    filterBarArea = b.removeFromTop (46);
    {
        auto r = filterBarArea.withTrimmedBottom (1).reduced (12, 8);
        refreshButton.setBounds (r.removeFromRight (widthOf (refreshButton, 22)));
        r.removeFromRight (8);

        sortArea = r.removeFromRight (190);
        {
            auto s = sortArea;
            sortDirectionButton.setBounds (s.removeFromRight (58));
            sortBox.setBounds (s.reduced (1));
        }
        r.removeFromRight (8);

        sourceArea = {};
        if (builtInToggle.isVisible())
        {
            sourceArea = r.removeFromRight (widthOf (builtInToggle, 22) + widthOf (userToggle, 22));
            auto s = sourceArea.reduced (1);
            builtInToggle.setBounds (s.removeFromLeft (widthOf (builtInToggle, 22)));
            userToggle.setBounds (s);
            r.removeFromRight (8);
        }
        searchBox.setBounds (r);
    }

    extraFiltersArea = {};
    if (extras.extraFilters != nullptr && extras.extraFiltersHeight > 0)
    {
        extraFiltersArea = b.removeFromTop (extras.extraFiltersHeight + 9);
        extras.extraFilters->setBounds (extraFiltersArea.withTrimmedBottom (1).reduced (12, 4));
    }

    if (activeFilterBar.isVisible())
        activeFilterBar.setBounds (b.removeFromTop (ActiveFilterBar::height));

    // ── status bar ──
    statusBarArea = b.removeFromBottom (42);
    {
        auto r = statusBarArea.withTrimmedTop (1).reduced (12, 7);
        saveAsButton.setBounds (r.removeFromRight (widthOf (saveAsButton, 22)));
        r.removeFromRight (8);
        newButton.setBounds (r.removeFromRight (widthOf (newButton, 22)));
        r.removeFromRight (10);
        statusLabel.setBounds (r.withTrimmedLeft (-4));
    }

    // ── rail and list ──
    if (options.showTagRail)
        tagRail.setBounds (b.removeFromLeft (TagRail::width));
    liveStrip.setBounds (b.removeFromTop (LiveStrip::height));
    list.setBounds (b);

    if (modal != nullptr) modal->setBounds (getLocalBounds());
}

bool PresetPanel::keyPressed (const juce::KeyPress& k)
{
    // What reaches here is what no field or list claimed: `/` is only a filter shortcut when
    // nothing that types has the keyboard.
    if (k.getTextCharacter() == '/')
    {
        searchBox.grabKeyboardFocus();
        return true;
    }
    if (k == juce::KeyPress::escapeKey && editTarget != EditTarget::none)
    {
        closeEditor();
        return true;
    }
    return false;
}

// ── status ──────────────────────────────────────────────────────────────────

void PresetPanel::setStatus (const juce::String& s)
{
    status = s;
    statusLabel.setText (s, juce::dontSendNotification);
}

void PresetPanel::refreshFolderLabel()
{
    const auto folder = host.presetFolder();
    folderLabel.setText (folder.getFullPathName(), juce::dontSendNotification);
    folderLabel.setTooltip (folder.getFullPathName());
}

void PresetPanel::timerCallback()
{
    if (! isShowing()) return;
    if (isDirty() != lastDirty) refreshLiveStrip();
}

// ── scanning and filtering ──────────────────────────────────────────────────

void PresetPanel::rescanAndRefreshList()
{
    library.setPresetDir (host.presetFolder());
    library.rescan();
    refreshTagViews();
    applyFilterAndRefreshList();
}

void PresetPanel::refreshTagViews()
{
    // A tag that no preset carries any more cannot filter anything but to nothing.
    const auto counts = library.getTagCounts();
    for (int i = activeTags.size(); --i >= 0;)
    {
        bool exists = false;
        for (const auto& c : counts)
            if (c.tag == activeTags[i]) { exists = true; break; }
        if (! exists) activeTags.remove (i);
    }

    tagRail.setTags (counts, activeTags);
    tagRail.setVisible (options.showTagRail);

    const bool showBar = ! activeTags.isEmpty();
    activeFilterBar.setTags (activeTags, filter.matchAnyTag);
    if (showBar != activeFilterBar.isVisible())
    {
        activeFilterBar.setVisible (showBar);
        resized();
    }
}

void PresetPanel::applyFilterAndRefreshList()
{
    filter.text = searchBox.getText().trim();
    filter.requiredTags = activeTags;

    // The two source toggles only earn their space in a plugin that ships built-ins.
    bool anyBuiltIn = false;
    for (const auto& e : library.getAll())
        if (e.isBuiltIn) { anyBuiltIn = true; break; }
    if (builtInToggle.isVisible() != anyBuiltIn)
    {
        builtInToggle.setVisible (anyBuiltIn);
        userToggle.setVisible (anyBuiltIn);
        resized();
        repaint();
    }

    filter.includeBuiltIn = ! anyBuiltIn || builtInToggle.getToggleState();
    filter.includeUser    = ! anyBuiltIn || userToggle.getToggleState();
    filter.extraPredicate = extras.extraFilterPredicate;

    rows = library.getFiltered (filter);
    sortPresets (rows, sortKey, sortAscending,
                 [this] (const PresetEntry& e) { return favourites.contains (favouriteKey (e)); });

    sortDirectionButton.setButtonText (sortAscending ? juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x91 A\xe2\x80\x93Z"))
                                                     : juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x93 Z\xe2\x80\x93" "A")));
    sortDirectionButton.setTooltip (sortAscending ? "ascending " + emDash + " click for descending"
                                                  : "descending " + emDash + " click for ascending");

    list.updateContent();

    // The selection follows its preset, not its index: a rescan or a re-sort moves rows about.
    const int selected = rowIndexOf (selectedKey);
    if (selected < 0 && selectedKey.isNotEmpty())
    {
        selectedKey = {};
        stopAuditionIfRunning();
        if (onSelectionChanged) onSelectionChanged (nullptr);
    }
    list.selectRow (selected, false);

    // So does the popover.
    if (editTarget == EditTarget::row)
    {
        const int row = rowIndexOf (editKey);
        if (row < 0) closeEditor();
        else         list.showPopoverForRow (row);
    }
    else if (editTarget == EditTarget::live || editTarget == EditTarget::saveAs)
    {
        list.showPopoverForLive();
    }

    refreshLiveStrip();
}

const PresetEntry* PresetPanel::findEntry (const juce::String& key) const
{
    if (key.isEmpty()) return nullptr;
    for (const auto& e : library.getAll())
        if (e.key() == key) return &e;
    return nullptr;
}

int PresetPanel::rowIndexOf (const juce::String& key) const
{
    if (key.isEmpty()) return -1;
    for (size_t i = 0; i < rows.size(); ++i)
        if (rows[i]->key() == key) return (int) i;
    return -1;
}

void PresetPanel::selectKey (const juce::String& key)
{
    selectedKey = key;
    const int row = rowIndexOf (key);
    list.selectRow (row, false);
    if (onSelectionChanged) onSelectionChanged (row >= 0 ? rows[(size_t) row] : nullptr);
}

juce::String PresetPanel::favouriteKey (const PresetEntry& e) const
{
    // Relative to the preset folder, so a favourite survives the folder being migrated.
    if (e.isBuiltIn) return e.key();
    const auto dir = library.getPresetDir();
    return "file:" + (e.file.isAChildOf (dir) ? e.file.getRelativePathFrom (dir).replaceCharacter ('\\', '/')
                                              : e.file.getFullPathName());
}

const PresetEntry* PresetPanel::openEntry() const { return findEntry (currentKey); }

// ── list model ──────────────────────────────────────────────────────────────

const PresetEntry* PresetPanel::getRow (int row)
{
    return juce::isPositiveAndBelow (row, (int) rows.size()) ? rows[(size_t) row] : nullptr;
}

bool PresetPanel::isRowOpen (const PresetEntry& e)
{
    // By key, not by file: a built-in has no file, so comparing files would mark every built-in
    // as the open one whenever nothing is open.
    return currentKey.isNotEmpty() && e.key() == currentKey;
}

bool PresetPanel::isRowFavourite (const PresetEntry& e) { return favourites.contains (favouriteKey (e)); }
bool PresetPanel::isTagFiltering (const juce::String& t) { return activeTags.contains (t); }

bool PresetPanel::canAudition (const PresetEntry& e)
{
    return auditioner != nullptr && auditioner->canAudition (e);
}

void PresetPanel::rowSelected (int row)
{
    // Whatever was previewing was previewing the row being left.
    stopAuditionIfRunning();

    const auto* e = getRow (row);
    selectedKey = e != nullptr ? e->key() : juce::String();

    if (editTarget == EditTarget::row && editKey != selectedKey)
        closeEditor();

    if (onSelectionChanged) onSelectionChanged (e);
}

void PresetPanel::rowLoadRequested (int row)
{
    if (const auto* e = getRow (row)) requestLoad (*e);
}

void PresetPanel::rowEditRequested (int row) { openEditorForRow (row); }

void PresetPanel::rowDuplicateRequested (int row)
{
    const auto* e = getRow (row);
    if (e == nullptr) return;
    const auto entry = *e;   // the library rescans under it

    juce::File copy;
    juce::String err;
    if (! library.duplicatePreset (entry, copy, err))
    {
        showError (err);
        return;
    }

    selectedKey = copy.getFullPathName();
    rescanAndRefreshList();
    setStatus ("duplicated as " + quoted (copy.getFileNameWithoutExtension()));
}

void PresetPanel::rowDeleteRequested (int row)
{
    const auto* e = getRow (row);
    if (e == nullptr) return;
    const auto entry = *e;

    if (entry.isBuiltIn)
    {
        setStatus ("factory presets cannot be deleted");
        return;
    }

    if (! options.confirmDelete)
    {
        deleteNow (entry);
        return;
    }

    auto& m = showModal (440);
    m.setTitle ("Delete " + quoted (displayName (entry)) + "?");
    m.addText ("Moved to the system trash, not erased.");
    m.addButton ("delete", ButtonStyle::destructive, [this, entry]
    {
        const auto target = entry;   // this lambda goes with the modal
        closeModal();
        deleteNow (target);
    });
    m.addCancel ("cancel", [this] { closeModal(); });
    finishModal();
}

void PresetPanel::deleteNow (PresetEntry entry)
{
    juce::String err;
    const auto favourite = favouriteKey (entry);
    const auto liveBefore = liveMetadata();   // read while the file is still there to read it from
    if (! library.deletePreset (entry, err))
    {
        showError (err);
        return;
    }

    if (entry.key() == currentKey)
    {
        // Its on-disk identity is gone, but the live patch is not: it keeps the name, so the
        // strip still says what it is and the next save writes it back.
        pendingLiveMeta = liveBefore;
        currentFile = juce::File();
        currentKey = {};
    }
    if (editKey == entry.key()) closeEditor();
    if (favourites.contains (favourite))
    {
        favourites.removeString (favourite);
        if (onFavouritesChanged) onFavouritesChanged();
    }

    rescanAndRefreshList();
    setStatus (juce::String ("deleted ") + emDash + " moved to trash");
}

void PresetPanel::rowAuditionRequested (int row)
{
    const auto* e = getRow (row);
    if (auditioner == nullptr || e == nullptr) return;
    // One button for both: the plugin owns the "is it playing" state, and asking it to stop
    // something already stopped is defined to be harmless.
    auditioner->stopAudition();
    if (auditioner->canAudition (*e))
        auditioner->startAudition (*e);
}

void PresetPanel::rowRevealRequested (int row)
{
    if (const auto* e = getRow (row); e != nullptr && ! e->isBuiltIn)
        e->file.revealToUser();
}

void PresetPanel::rowFavouriteToggled (int row)
{
    const auto* e = getRow (row);
    if (e == nullptr) return;

    const auto key = favouriteKey (*e);
    if (favourites.contains (key)) favourites.removeString (key);
    else                           favourites.add (key);

    if (sortKey == PresetSortKey::favouritesFirst) applyFilterAndRefreshList();
    else                                           list.repaint();
    if (onFavouritesChanged) onFavouritesChanged();
}

void PresetPanel::rowTagClicked (const juce::String& tag)
{
    if (activeTags.contains (tag)) activeTags.removeString (tag);
    else                           activeTags.add (tag);
    refreshTagViews();
    applyFilterAndRefreshList();
}

void PresetPanel::rowDragStarted (int row)
{
    list.setDraggingRow (row);
}

void PresetPanel::dragOperationEnded (const juce::DragAndDropTarget::SourceDetails&)
{
    list.setDraggingRow (-1);
}

void PresetPanel::escapePressed()
{
    if (editTarget != EditTarget::none) closeEditor();
}

void PresetPanel::stopAuditionIfRunning()
{
    if (auditioner != nullptr)
        auditioner->stopAudition();
}

// ── tags ────────────────────────────────────────────────────────────────────

void PresetPanel::assignTag (const juce::String& tag, const juce::String& key)
{
    list.setDraggingRow (-1);
    const auto* found = findEntry (key);
    if (found == nullptr) return;
    auto entry = *found;
    const auto name = displayName (entry);

    if (entry.isBuiltIn)
    {
        setStatus (quoted (name) + " is a factory preset " + emDash + " duplicate it to tag it");
        return;
    }
    if (entry.meta.tags.contains (tag))
    {
        setStatus (quoted (name) + " already tagged " + tag);
        return;
    }

    auto meta = entry.meta;
    meta.name = name;   // a tag is not a rename
    meta.tags.add (tag);

    juce::String err;
    if (! library.renamePreset (entry, meta, err))
    {
        showError (err);
        return;
    }
    if (entry.key() == currentKey) pendingLiveMeta = entry.meta;

    rescanAndRefreshList();
    setStatus ("tagged " + quoted (name) + " as " + tag);
}

// ── the live patch ──────────────────────────────────────────────────────────

PresetMetadata PresetPanel::liveMetadata (bool askDocument) const
{
    PresetMetadata m;
    if (const auto* e = openEntry(); e != nullptr)
    {
        m = e->meta;
        m.name = displayName (*e);
    }
    else
    {
        m = pendingLiveMeta;
    }

    // Serialising the live document can be expensive, so it is only asked for its name and author
    // when something is about to be written or edited -- never to repaint a strip.
    if (askDocument && (m.name.isEmpty() || m.author.isEmpty()))
    {
        const auto doc = host.captureDocument();
        if (m.name.isEmpty())   m.name = doc.getProperty (format.nameProperty, "").toString();
        if (m.author.isEmpty()) m.author = doc.getProperty (format.authorProperty, "").toString();
    }
    return m;
}

juce::String PresetPanel::liveName() const
{
    if (currentKey.isNotEmpty())
        return liveMetadata().name;
    // A new patch has no name until it is given one; the strip says so rather than inventing one.
    return pendingLiveMeta.name;
}

void PresetPanel::refreshLiveStrip()
{
    LiveStrip::State s;
    s.name = liveName();

    if (currentKey.isNotEmpty())
    {
        const auto m = liveMetadata();
        juce::StringArray parts;
        if (m.author.isNotEmpty()) parts.add (m.author);
        if (m.date.isNotEmpty()) parts.add (m.date.substring (0, 10));
        s.meta = parts.joinIntoString ("  " + middot + "  ");
    }

    s.dirty = isDirty();
    lastDirty = s.dirty;

    // A patch with no file of its own can always be saved -- that is how it gets one. A patch
    // with a file is saved only when there is something to save.
    s.canSave = ! liveHasFile() || s.dirty;
    s.saveIsAccent = s.canSave && s.dirty;
    s.canRevert = s.dirty && openEntry() != nullptr;
    liveStrip.setState (s);

    liveStrip.saveButton.setTooltip (liveHasFile()
        ? "write the live patch back to " + currentFile.getFileName()
        : "write the live patch into your folder as "
              + quoted (PresetFile::sanitiseFilename (liveName()) + format.extension));
}

// ── loading ─────────────────────────────────────────────────────────────────

bool PresetPanel::loadNeedsConfirmation (const PresetEntry* target) const
{
    switch (options.loadGuard)
    {
        case PresetPanelOptions::LoadGuard::none:  return false;
        case PresetPanelOptions::LoadGuard::dirty: return isDirty();
        case PresetPanelOptions::LoadGuard::always:
            // Reloading a clean open preset replaces nothing with anything different.
            return ! (target != nullptr && target->key() == currentKey && ! isDirty());
    }
    return true;
}

void PresetPanel::requestLoad (const PresetEntry& target)
{
    if (! loadNeedsConfirmation (&target))
    {
        loadEntry (target);
        return;
    }

    const auto entry = target;
    const bool dirty = isDirty();
    const auto live = liveName();

    auto& m = showModal (460);
    m.setTitle ("Load " + quoted (displayName (entry)) + "?");
    if (! dirty)
        m.addText ("Loading replaces the live patch.");
    else if (live.isNotEmpty())
        m.addText (quoted (live) + " has unsaved changes. Loading replaces the live patch and those "
                   "edits are gone.");
    else
        m.addText ("The live patch has unsaved changes and has never been saved. Loading replaces it.");

    // The one that loses nothing is the one that looks like the default.
    m.addButton ("save, then load", ButtonStyle::accent, [this, entry]
    {
        const auto e = entry;
        closeModal();
        if (doSave()) loadEntry (e);
    });
    m.addButton ("load anyway", ButtonStyle::plain, [this, entry]
    {
        const auto e = entry;
        closeModal();
        loadEntry (e);
    });
    m.addCancel ("cancel", [this] { closeModal(); });
    finishModal();
}

void PresetPanel::loadEntry (const PresetEntry& entryIn)
{
    const auto entry = entryIn;   // may point into the library, which a load's callbacks can rescan
    const auto label = displayName (entry);

    auto loaded = entry.isBuiltIn
        ? PresetFile::loadFromData (entry.data, entry.dataSize, entry.meta.name, format)
        : PresetFile::load (entry.file, format);

    if (! loaded.has_value())
    {
        showError ("Could not load " + quoted (label) + " -- it may be corrupt.");
        return;
    }

    stopAuditionIfRunning();
    host.applyDocument (loaded->tree);
    // A built-in has no file to write back to, so the open document has no on-disk identity and
    // Save writes a new file -- which is exactly what editing a built-in should do.
    currentFile = entry.file;
    currentKey = entry.key();
    pendingLiveMeta = loaded->meta;
    host.documentLoaded (entry.file, loaded->meta);

    if (editTarget == EditTarget::live || editTarget == EditTarget::saveAs) closeEditor();
    setStatus ("loaded " + quoted (label));
    list.relayout();   // the OPEN badge moved
    refreshLiveStrip();
}

void PresetPanel::doRevert()
{
    const auto* e = openEntry();
    if (e == nullptr)
    {
        setStatus ("nothing to revert");
        return;
    }
    const auto entry = *e;
    loadEntry (entry);
    setStatus ("reverted " + quoted (displayName (entry)) + " to disk");
}

// ── writing ─────────────────────────────────────────────────────────────────

bool PresetPanel::writeLiveTo (const juce::File& file, PresetMetadata meta)
{
    meta.date = juce::Time::getCurrentTime().toISO8601 (true);
    // A save writes the *live* document, so its listable extras are the live one's.
    meta.extra = host.captureExtraMetadata();

    if (! PresetFile::save (file, host.captureDocument(), meta, format))
    {
        showError ("Could not write " + quoted (file.getFullPathName()) + ".");
        return false;
    }

    auto saved = meta;
    saved.name = file.getFileNameWithoutExtension();   // the file had the last word
    host.documentSaved (file, saved);

    currentFile = file;
    currentKey = file.getFullPathName();
    pendingLiveMeta = saved;
    rescanAndRefreshList();
    setStatus ("saved " + quoted (saved.name));
    return true;
}

bool PresetPanel::doSave()
{
    auto meta = liveMetadata (true);

    if (liveHasFile())
        return writeLiveTo (currentFile, meta);

    // No file of its own -- a new patch, or one loaded from a built-in. It gets one under its own
    // name, and never by writing over a file it was not opened from.
    auto folder = host.presetFolder();
    folder.createDirectory();
    const auto target = PresetFile::resolveCollision (folder, PresetFile::sanitiseFilename (meta.name), format);
    return writeLiveTo (target, meta);
}

void PresetPanel::doSaveAs()
{
    if (editTarget == EditTarget::saveAs)
    {
        closeEditor();
        return;
    }

    // Save as asks for the name where every other name is asked for: in the popover, under the
    // live strip. The suggestion is the live name, moved on to the next free one.
    auto meta = liveMetadata (true);
    const auto folder = host.presetFolder();
    meta.name = PresetFile::resolveCollision (folder, PresetFile::sanitiseFilename (meta.name), format)
                    .getFileNameWithoutExtension();

    editTarget = EditTarget::saveAs;
    editKey = {};
    popover.setHeader (nullptr, 0);
    const auto name = liveName();
    popover.open ("save live patch as", name.isNotEmpty() ? name : juce::String ("new patch"), meta,
                  library.getAllTags(), PresetEditPopover::Mode::saveAs, true);
    list.showPopoverForLive();
    popover.focusName();
}

void PresetPanel::doNew()
{
    if (! loadNeedsConfirmation (nullptr))
    {
        newNow();
        return;
    }

    // New throws the live patch away as surely as a load does, so it passes the same guard.
    const auto live = liveName();
    auto& m = showModal (460);
    m.setTitle ("Start a new patch?");
    if (! isDirty())
        m.addText ("A new patch replaces the live one.");
    else if (live.isNotEmpty())
        m.addText (quoted (live) + " has unsaved changes. A new patch replaces it and those edits are gone.");
    else
        m.addText ("The live patch has unsaved changes and has never been saved. A new patch replaces it.");

    m.addButton ("save, then new", ButtonStyle::accent, [this]
    {
        closeModal();
        if (doSave()) newNow();
    });
    m.addButton ("new anyway", ButtonStyle::plain, [this]
    {
        closeModal();
        newNow();
    });
    m.addCancel ("cancel", [this] { closeModal(); });
    finishModal();
}

void PresetPanel::newNow()
{
    // Nothing is open afterwards, so nothing may be previewing either.
    stopAuditionIfRunning();
    host.newDocument();

    currentFile = juce::File();
    currentKey = {};
    pendingLiveMeta = {};

    if (editTarget == EditTarget::live || editTarget == EditTarget::saveAs) closeEditor();
    setStatus ("new patch");
    list.relayout();
    refreshLiveStrip();
}

// ── popover ─────────────────────────────────────────────────────────────────

void PresetPanel::openEditorForRow (int row)
{
    const auto* e = getRow (row);
    if (e == nullptr) return;

    if (selectedKey != e->key())
    {
        list.selectRow (row, false);
        rowSelected (row);
    }

    editTarget = EditTarget::row;
    editKey = e->key();

    auto meta = e->meta;
    meta.name = displayName (*e);
    popover.setHeader (extras.detailsHeader, extras.detailsHeaderHeight);
    popover.open ("editing", displayName (*e), meta, library.getAllTags(),
                  e->isBuiltIn ? PresetEditPopover::Mode::factoryCopy : PresetEditPopover::Mode::edit,
                  false);
    list.showPopoverForRow (row);
    popover.focusName();
}

void PresetPanel::openEditorForLive()
{
    if (editTarget == EditTarget::live)
    {
        closeEditor();
        return;
    }

    const auto* open = openEntry();
    const auto mode = open == nullptr ? PresetEditPopover::Mode::liveUnsaved
                    : open->isBuiltIn ? PresetEditPopover::Mode::factoryCopy
                                      : PresetEditPopover::Mode::edit;

    editTarget = EditTarget::live;
    editKey = currentKey;

    const auto name = liveName();
    popover.setHeader (nullptr, 0);
    popover.open ("editing live patch", name.isNotEmpty() ? name : juce::String ("new patch"),
                  liveMetadata (true), library.getAllTags(), mode, true);
    list.showPopoverForLive();
    popover.focusName();
}

void PresetPanel::closeEditor()
{
    const bool wasOpen = editTarget != EditTarget::none;
    editTarget = EditTarget::none;
    editKey = {};
    popover.setHeader (nullptr, 0);
    list.hidePopover();
    if (wasOpen && list.isShowing()) list.grabKeyboardFocus();
}

void PresetPanel::applyEditor()
{
    auto draft = popover.getDraft();

    switch (editTarget)
    {
        case EditTarget::none:
            return;

        case EditTarget::row:
        {
            const auto* e = findEntry (editKey);
            if (e == nullptr) { closeEditor(); return; }
            applyToEntry (*e, draft);
            return;
        }

        case EditTarget::saveAs:
        {
            auto folder = host.presetFolder();
            folder.createDirectory();
            const auto target = folder.getChildFile (PresetFile::sanitiseFilename (draft.name) + format.extension);

            if (target.existsAsFile() && target != currentFile)
            {
                // Replacing a file is what a native save dialog's overwrite warning used to ask about.
                auto& m = showModal (460);
                m.setTitle ("Replace " + quoted (target.getFileNameWithoutExtension()) + "?");
                m.addText (quoted (target.getFileName()) + " already exists in your folder. Replacing it "
                           "writes the live patch over it.");
                m.addButton ("replace", ButtonStyle::destructive, [this, target, draft]
                {
                    const auto t = target;
                    const auto d = draft;
                    closeModal();
                    if (writeLiveTo (t, d)) closeEditor();
                });
                m.addCancel ("cancel", [this] { closeModal(); });
                finishModal();
                return;
            }

            if (writeLiveTo (target, draft)) closeEditor();
            return;
        }

        case EditTarget::live:
        {
            const auto* open = openEntry();

            if (open != nullptr && ! open->isBuiltIn)
            {
                applyToEntry (*open, draft);
                return;
            }

            if (open != nullptr)
            {
                // The live patch came from a built-in, which cannot be written to: its editable
                // copy is the live patch itself, saved into the folder under the new name.
                auto folder = host.presetFolder();
                folder.createDirectory();
                const auto target = PresetFile::resolveCollision (
                    folder, PresetFile::sanitiseFilename (draft.name), format);
                if (writeLiveTo (target, draft))
                {
                    closeEditor();
                    setStatus ("saved the live patch as " + quoted (target.getFileNameWithoutExtension()));
                }
                return;
            }

            // Never saved: this names it, and save writes it.
            draft.name = PresetFile::sanitiseFilename (draft.name);
            pendingLiveMeta = draft;
            closeEditor();
            refreshLiveStrip();
            setStatus ("named the live patch " + quoted (draft.name) + " " + emDash + " save writes it");
            return;
        }
    }
}

void PresetPanel::applyToEntry (PresetEntry entry, PresetMetadata draft)
{
    draft.name = PresetFile::sanitiseFilename (draft.name.isNotEmpty() ? draft.name : displayName (entry));
    juce::String err;

    if (entry.isBuiltIn)
    {
        // A built-in cannot be written to, so applying writes an editable copy rather than failing.
        PresetEntry copy;
        if (! library.writeEditableCopy (entry, draft, copy, err))
        {
            showError (err);
            rescanAndRefreshList();
            return;
        }
        closeEditor();
        selectedKey = copy.key();
        rescanAndRefreshList();
        setStatus ("saved a copy of the factory preset as " + quoted (displayName (copy)));
        return;
    }

    const auto oldFile = entry.file;
    const auto oldFavourite = favouriteKey (entry);
    const bool wasOpen = entry.key() == currentKey;
    const bool wasSelected = entry.key() == selectedKey;

    // An edit rewrites a file that may not be the open document -- so its extras stay the ones
    // it was saved with rather than being restamped from the live document.
    draft.extra = entry.meta.extra;

    if (! library.renamePreset (entry, draft, err))
    {
        showError (err);
        return;
    }

    const bool renamed = entry.file != oldFile;

    // If this was the file the live document came from, keep it and this panel in step -- or the
    // next plain save would write the old name straight back over the rename.
    if (wasOpen)
    {
        currentFile = entry.file;
        currentKey = entry.key();
        pendingLiveMeta = entry.meta;
        host.documentSaved (entry.file, entry.meta);
    }
    if (wasSelected) selectedKey = entry.key();
    if (renamed && favourites.contains (oldFavourite))
    {
        favourites.set (favourites.indexOf (oldFavourite), favouriteKey (entry));
        if (onFavouritesChanged) onFavouritesChanged();
    }

    closeEditor();
    rescanAndRefreshList();
    setStatus (renamed ? "renamed to " + quoted (entry.file.getFileNameWithoutExtension()) + " and wrote metadata"
                       : "wrote metadata for " + quoted (entry.file.getFileNameWithoutExtension()));
}

// ── folder ──────────────────────────────────────────────────────────────────

void PresetPanel::choosePresetFolder()
{
    folderChooser = std::make_unique<juce::FileChooser> ("Choose a folder for presets", host.presetFolder());
    folderChooser->launchAsync (juce::FileBrowserComponent::openMode
                                    | juce::FileBrowserComponent::canSelectDirectories,
                                [this] (const juce::FileChooser& fc)
    {
        const auto dir = fc.getResult();
        if (dir == juce::File() || ! dir.isDirectory()) return;

        host.setPresetFolder (dir);
        refreshFolderLabel();
        // The list is still showing the old folder's presets until this runs.
        rescanAndRefreshList();
        setStatus (juce::String ("preset folder changed ") + emDash + " nothing was moved");
    });
    // `folderChooser` is a member unique_ptr, so it (and the chooser it owns) lives as long as this
    // panel does -- which is what makes the raw `this` capture above safe.
}

void PresetPanel::openMigrate()
{
    const auto from = host.presetFolder();
    const int count = PresetFile::countPresetFiles (from, format);
    bool anyBuiltIn = false;
    for (const auto& e : library.getAll())
        if (e.isBuiltIn) { anyBuiltIn = true; break; }

    auto& m = showModal (540);
    m.setTitle ("Migrate preset folder");
    m.addCaption ("from");
    m.addPathBox (from.getFullPathName());
    m.addCaption ("to");
    juce::TextButton* browse = nullptr;
    auto& dest = m.addPathInput ("choose a destination folder" + ellipsis, browse);
    auto& summary = m.addSummary ({});
    auto& move = m.addButton ("move presets", ButtonStyle::accent, {});
    m.addCancel ("cancel", [this] { closeModal(); });

    auto destination = [&dest]
    {
        const auto text = dest.getText().trim();
        return juce::File::isAbsolutePath (text) ? juce::File (text) : juce::File();
    };

    // Everything below lives in the modal, and so do the callbacks that use it.
    auto update = [&dest, &summary, &move, from, count, anyBuiltIn, destination]
    {
        juce::ignoreUnused (dest);
        const auto to = destination();
        juce::String text;
        bool ok = false;

        if (to == juce::File())
            text = "Pick a destination. Every preset file in the current folder is moved there, then "
                   "the library points at the new folder.";
        else if (to == from)
            text = "That is the current folder. Pick another one.";
        else if (to.isAChildOf (from))
            text = "That folder is inside the current one. Pick one outside it.";
        else
        {
            ok = true;
            text = plural (count, "user preset") + (count == 1 ? " will be moved." : " will be moved.");
            if (anyBuiltIn) text << " Factory presets stay in the plugin.";
            text << " The library then points at the new folder.";
        }

        summary.setText (text, juce::dontSendNotification);
        move.setEnabled (ok);
    };

    dest.onTextChange = update;
    move.onClick = [this, destination]
    {
        const auto to = destination();
        closeModal();
        runMigrate (to);
    };
    browse->onClick = [this, safeDest = juce::Component::SafePointer<juce::TextEditor> (&dest), from]
    {
        folderChooser = std::make_unique<juce::FileChooser> ("Choose where to move your presets",
                                                             from.getParentDirectory());
        folderChooser->launchAsync (juce::FileBrowserComponent::openMode
                                        | juce::FileBrowserComponent::canSelectDirectories,
                                    [safeDest] (const juce::FileChooser& fc)
        {
            const auto dir = fc.getResult();
            if (safeDest != nullptr && dir != juce::File())
                safeDest->setText (dir.getFullPathName(), true);
        });
    };

    update();
    finishModal();
    if (dest.isShowing()) dest.grabKeyboardFocus();
}

void PresetPanel::runMigrate (const juce::File& dest)
{
    const auto oldFolder = host.presetFolder();
    const auto result = host.migratePresetFolder (dest, format);

    // The open preset moved with the rest; follow it, or the next save would recreate it where
    // it used to be.
    for (int i = 0; i < result.movedFrom.size() && i < result.moved.size(); ++i)
    {
        if (result.movedFrom[i] == currentFile)
        {
            currentFile = result.moved[i];
            currentKey = currentFile.getFullPathName();
        }
        if (result.movedFrom[i].getFullPathName() == selectedKey)
            selectedKey = result.moved[i].getFullPathName();
    }

    refreshFolderLabel();
    rescanAndRefreshList();

    if (! result.ok)
    {
        setStatus ("migration stopped " + emDash + " moved " + plural (result.moved.size(), "preset"));
        auto& m = showModal (500);
        m.setTitle ("Migration stopped");
        m.addText (result.error);
        m.addText (result.moved.isEmpty()
                       ? juce::String ("Nothing was moved. The library still points at the old folder.")
                       : plural (result.moved.size(), "preset") + " had already moved to the new folder "
                             "before it stopped. The library still points at the old folder.");
        if (! result.moved.isEmpty())
        {
            juce::StringArray names;
            for (const auto& f : result.moved) names.add (f.getFileName());
            m.addText ("moved: " + names.joinIntoString (", "));
        }
        m.addButton ("ok", ButtonStyle::accent, [this] { closeModal(); });
        finishModal();
        return;
    }

    setStatus ("moved " + plural (result.moved.size(), "preset"));
    showMigrateDone (oldFolder, result);
}

void PresetPanel::showMigrateDone (const juce::File& oldFolder, const MigrationResult& result)
{
    auto& m = showModal (500);
    m.setTitle ("Moved " + plural (result.moved.size(), "preset"));

    if (result.sourceHasOtherContent)
    {
        // Something that is not a preset is still in there, and it is not this panel's to throw away.
        m.addText ("The library now points at the new folder. The old one still holds files that "
                   "are not presets, so it is kept.");
        m.addPathBox (oldFolder.getFullPathName());
        if (! result.skipped.isEmpty())
        {
            juce::StringArray shown;
            for (int i = 0; i < juce::jmin (4, result.skipped.size()); ++i) shown.add (result.skipped[i]);
            auto text = "left behind: " + shown.joinIntoString (", ");
            if (result.skipped.size() > shown.size())
                text << " and " << (result.skipped.size() - shown.size()) << " more";
            m.addText (text);
        }
        m.addButton ("keep it", ButtonStyle::accent, [this]
        {
            closeModal();
            setStatus ("migrated " + middot + " old folder kept");
        });
        finishModal();
        return;
    }

    m.addText ("The library now points at the new folder. Delete the old one?");
    m.addPathBox (oldFolder.getFullPathName());
    // Deleting is never implied by migrating, and the destructive option is never the emphasised one.
    m.addButton ("keep it", ButtonStyle::accent, [this]
    {
        closeModal();
        setStatus ("migrated " + middot + " old folder kept");
    });
    m.addButton ("delete old folder", ButtonStyle::destructive, [this, oldFolder]
    {
        const auto folder = oldFolder;
        closeModal();
        // To the trash, never a hard delete: it is the user's folder, and it was theirs first.
        if (folder.moveToTrash())
            setStatus ("migrated " + middot + " old folder moved to trash");
        else
            showError ("Could not move " + quoted (folder.getFullPathName()) + " to the trash. It has been kept.");
    });
    finishModal();
}

// ── modal ───────────────────────────────────────────────────────────────────

PresetModal& PresetPanel::showModal (int cardWidth)
{
    if (modal != nullptr) closeModal();
    modal = std::make_unique<PresetModal> (palette, cardWidth);
    addAndMakeVisible (*modal);
    modal->setBounds (getLocalBounds());
    return *modal;
}

void PresetPanel::finishModal()
{
    if (modal == nullptr) return;
    modal->resized();
    modal->toFront (false);
    if (modal->isShowing()) modal->grabKeyboardFocus();
}

void PresetPanel::closeModal()
{
    if (modal == nullptr) return;
    modal->setVisible (false);
    removeChildComponent (modal.get());
    retiredModal = std::move (modal);
}

void PresetPanel::showError (const juce::String& message)
{
    setStatus (message);
    auto& m = showModal (440);
    m.setTitle ("Preset librarian");
    m.addText (message);
    m.addButton ("ok", ButtonStyle::accent, [this] { closeModal(); });
    finishModal();
}

} // namespace atl
