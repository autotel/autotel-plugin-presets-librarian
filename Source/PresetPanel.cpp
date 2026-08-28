#include "PresetPanel.h"

namespace atl
{

// ── list model ────────────────────────────────────────────────────────────────

int PresetPanel::ListModel::getNumRows() { return (int) owner.rows.size(); }

void PresetPanel::ListModel::paintListBoxItem (int row, juce::Graphics& g, int width, int height,
                                               bool selected)
{
    if (! juce::isPositiveAndBelow (row, (int) owner.rows.size())) return;
    const auto& e = *owner.rows[(size_t) row];
    const auto& c = owner.colours;

    if (selected)
    {
        g.setColour (c.accent.withAlpha (0.25f));
        g.fillRect (0, 0, width, height);
    }

    // By key, not by file: a built-in has no file, so comparing files would mark every built-in
    // as the open one whenever nothing is open.
    const bool isOpen = ! owner.currentKey.isEmpty() && e.key() == owner.currentKey;
    g.setColour (isOpen ? c.accent : (e.isBuiltIn ? c.text.withAlpha (0.75f) : c.text));
    g.setFont (juce::FontOptions (12.5f, juce::Font::bold));

    juce::String title;
    if (isOpen) title += juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb8 "));
    // A padlock, because a built-in row offers everything except writing to it.
    if (e.isBuiltIn) title += juce::String (juce::CharPointer_UTF8 ("\xf0\x9f\x94\x92 "));
    // The filename is the name (see PresetMetadata), so it is what the row says.
    title += e.isBuiltIn ? e.meta.name : e.file.getFileNameWithoutExtension();
    g.drawText (title, 8, 1, width - 12, height / 2, juce::Justification::centredLeft, true);

    juce::StringArray subParts;
    if (e.category.isNotEmpty()) subParts.add (e.category);
    // First of the user's own, because it is the note they left themselves about *this* file.
    if (e.meta.comment.isNotEmpty()) subParts.add (e.meta.comment);
    if (e.meta.author.isNotEmpty()) subParts.add (e.meta.author);
    if (! e.meta.tags.isEmpty()) subParts.add (e.meta.tags.joinIntoString (", "));
    if (e.meta.date.isNotEmpty()) subParts.add (e.meta.date.substring (0, 10));   // just the date

    g.setColour (c.textDim);
    g.setFont (juce::FontOptions (10.0f));
    g.drawText (subParts.joinIntoString ("   \xc2\xb7   "), 8, height / 2, width - 12,
                height - height / 2, juce::Justification::centredLeft, true);
}

void PresetPanel::ListModel::selectedRowsChanged (int lastRowSelected)
{
    owner.selectionChanged (lastRowSelected);
}

void PresetPanel::ListModel::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) owner.showRowMenu (row);
}

// ── PresetPanel ──────────────────────────────────────────────────────────────

PresetPanel::PresetPanel (PresetHost& h, PresetFormat f)
    : host (h), format (std::move (f)), library (format)
{
    extensionLabel.setText (format.extension, juce::dontSendNotification);

    searchBox.setTextToShowWhenEmpty ("filter...", colours.textDim);
    searchBox.onTextChange = [this] { applyFilterAndRefreshList(); };
    addAndMakeVisible (searchBox);

    tagFilterBox.onChange = [this] { applyFilterAndRefreshList(); };
    addAndMakeVisible (tagFilterBox);

    refreshButton.onClick = [this] { rescanAndRefreshList(); };
    addAndMakeVisible (refreshButton);

    builtInToggle.setToggleState (true, juce::dontSendNotification);
    userToggle.setToggleState (true, juce::dontSendNotification);
    for (auto* t : { &builtInToggle, &userToggle })
    {
        t->onClick = [this] { applyFilterAndRefreshList(); };
        // Made visible only once a scan has found built-ins -- see applyFilterAndRefreshList.
        addChildComponent (*t);
    }

    auditionButton.onClick = [this] { toggleAudition(); };
    addChildComponent (auditionButton);   // shown by setAuditioner

    list.setModel (&listModel);
    list.setRowHeight (30);
    addAndMakeVisible (list);

    statusLabel.setFont (juce::FontOptions (10.5f));
    addAndMakeVisible (statusLabel);

    folderButton.setTooltip ("where presets are saved and listed");
    folderButton.onClick = [this] { choosePresetFolder(); };
    addAndMakeVisible (folderButton);

    folderLabel.setFont (juce::FontOptions (10.0f));
    addAndMakeVisible (folderLabel);

    for (auto* l : { &nameLabel, &authorLabel, &commentLabel, &descriptionLabel, &tagsLabel,
                     &dateLabel, &extensionLabel })
        addAndMakeVisible (*l);
    for (auto* e : { &nameEditor, &authorEditor, &commentEditor, &descriptionEditor, &tagsEditor })
        addAndMakeVisible (*e);

    nameEditor.setTooltip ("the file's name -- editing it renames the preset. Save is off while "
                           "this does not match the open file; Save As writes the new name.");
    // Editing the name is what turns Save off, so the button has to follow every keystroke.
    nameEditor.onTextChange = [this] { updateEnablement(); };
    nameEditor.onReturnKey = [this] { doRename(); };
    extensionLabel.setTooltip ("every preset is a " + format.extension + " file");

    commentEditor.setTooltip ("a note to yourself that a filename cannot carry -- \"this is the "
                              "newer one\". Shown in the list, never part of the name.");
    descriptionEditor.setMultiLine (true, true);
    descriptionEditor.setReturnKeyStartsNewLine (true);

    newButton.setTooltip ("start from an empty preset: the live document is cleared and no file "
                          "is open, so Save As is how it gets a name");
    newButton.onClick = [this] { doNew(); };
    addAndMakeVisible (newButton);
    saveButton.onClick = [this] { doSave(); };
    addAndMakeVisible (saveButton);
    saveAsButton.onClick = [this] { doSaveAs(); };
    addAndMakeVisible (saveAsButton);
    renameButton.onClick = [this] { doRename(); };
    addAndMakeVisible (renameButton);
    deleteButton.onClick = [this] { doDelete(); };
    addAndMakeVisible (deleteButton);

    applyColours();
    rescanAndRefreshList();
    updateDetailsFromSelection();
    updateEnablement();
    refreshFolderLabel();
}

PresetPanel::~PresetPanel()
{
    stopAuditionIfRunning();
}

void PresetPanel::setExtras (PresetPanelExtras e)
{
    if (extras.detailsHeader != nullptr) removeChildComponent (extras.detailsHeader);
    if (extras.extraFilters != nullptr)  removeChildComponent (extras.extraFilters);

    extras = std::move (e);

    if (extras.detailsHeader != nullptr) addAndMakeVisible (*extras.detailsHeader);
    if (extras.extraFilters != nullptr)  addAndMakeVisible (*extras.extraFilters);

    resized();
    applyFilterAndRefreshList();
}

void PresetPanel::setAuditioner (PresetAuditioner* a)
{
    if (auditioner != nullptr && auditioner != a)
        auditioner->stopAudition();

    auditioner = a;
    auditionButton.setVisible (auditioner != nullptr);
    updateEnablement();
    resized();
}

void PresetPanel::refilter()
{
    applyFilterAndRefreshList();
}

void PresetPanel::rescan()
{
    rescanAndRefreshList();
}

void PresetPanel::setBuiltIns (std::vector<BuiltInPreset> builtIns)
{
    library.setBuiltIns (std::move (builtIns));
    rescanAndRefreshList();
}

void PresetPanel::stopAuditionIfRunning()
{
    if (auditioner != nullptr)
        auditioner->stopAudition();
}

void PresetPanel::toggleAudition()
{
    if (auditioner == nullptr || ! haveSelection) return;
    // One button for both: the plugin owns the "is it playing" state, and asking it to stop
    // something already stopped is defined to be harmless.
    auditioner->stopAudition();
    if (auditioner->canAudition (selectedCopy))
        auditioner->startAudition (selectedCopy);
}

bool PresetPanel::selectionIsWritable() const
{
    return haveSelection && ! selectedCopy.isBuiltIn;
}

void PresetPanel::setColours (PresetPanelColours c)
{
    colours = c;
    applyColours();
    repaint();
}

void PresetPanel::applyColours()
{
    for (auto* e : { &searchBox, &nameEditor, &authorEditor, &commentEditor, &descriptionEditor,
                     &tagsEditor })
    {
        e->setColour (juce::TextEditor::backgroundColourId, colours.panel);
        e->setColour (juce::TextEditor::textColourId, colours.text);
        e->setColour (juce::TextEditor::outlineColourId, colours.outline);
        e->setFont (juce::FontOptions (12.0f));
    }

    for (auto* l : { &nameLabel, &extensionLabel, &authorLabel, &commentLabel, &descriptionLabel,
                     &tagsLabel, &dateLabel, &statusLabel, &folderLabel })
    {
        l->setColour (juce::Label::textColourId, colours.textDim);
        if (l != &statusLabel && l != &folderLabel) l->setFont (juce::FontOptions (10.5f));
    }

    for (auto* t : { &builtInToggle, &userToggle })
    {
        t->setColour (juce::ToggleButton::textColourId, colours.textDim);
        t->setColour (juce::ToggleButton::tickColourId, colours.accent);
        t->setColour (juce::ToggleButton::tickDisabledColourId, colours.outline);
    }

    searchBox.setTextToShowWhenEmpty ("filter...", colours.textDim);
    list.setColour (juce::ListBox::backgroundColourId, colours.background);
}

void PresetPanel::refreshFolderLabel()
{
    const auto folder = host.presetFolder();
    folderLabel.setText (folder.getFullPathName(), juce::dontSendNotification);
    folderLabel.setTooltip (folder.getFullPathName());
}

void PresetPanel::choosePresetFolder()
{
    folderChooser = std::make_unique<juce::FileChooser> ("Choose a folder for presets");
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
    });
    // `folderChooser` is a member unique_ptr, so it (and the chooser it owns) lives as long as this
    // panel does -- which is what makes the raw `this` capture above safe.
}

void PresetPanel::rescanAndRefreshList()
{
    library.setPresetDir (host.presetFolder());
    library.rescan();

    const auto keepTag = tagFilterBox.getText();
    tagFilterBox.clear (juce::dontSendNotification);
    tagFilterBox.addItem ("any tag", 1);
    int id = 2;
    for (const auto& t : library.getAllTags())
        tagFilterBox.addItem (t, id++);
    tagFilterBox.setText (keepTag.isEmpty() ? "any tag" : keepTag, juce::dontSendNotification);

    applyFilterAndRefreshList();
}

void PresetPanel::applyFilterAndRefreshList()
{
    filter.text = searchBox.getText();
    filter.requiredTags.clear();
    if (tagFilterBox.getText().isNotEmpty() && tagFilterBox.getText() != "any tag")
        filter.requiredTags.add (tagFilterBox.getText());

    // The two source toggles only earn their space in a plugin that ships built-ins.
    bool anyBuiltIn = false;
    for (const auto& e : library.getAll())
        if (e.isBuiltIn) { anyBuiltIn = true; break; }
    const bool togglesWereVisible = builtInToggle.isVisible();
    builtInToggle.setVisible (anyBuiltIn);
    userToggle.setVisible (anyBuiltIn);
    if (togglesWereVisible != anyBuiltIn) resized();

    filter.includeBuiltIn = ! anyBuiltIn || builtInToggle.getToggleState();
    filter.includeUser    = ! anyBuiltIn || userToggle.getToggleState();
    filter.extraPredicate = extras.extraFilterPredicate;

    const auto previousKey = haveSelection ? selectedCopy.key() : juce::String();

    rows = library.getFiltered (filter);
    list.updateContent();

    int newRow = -1;
    if (previousKey.isNotEmpty())
    {
        for (size_t i = 0; i < rows.size(); ++i)
            if (rows[i]->key() == previousKey) { newRow = (int) i; break; }
    }
    else if (currentKey.isNotEmpty())
    {
        for (size_t i = 0; i < rows.size(); ++i)
            if (rows[i]->key() == currentKey) { newRow = (int) i; break; }
    }

    list.selectRow (newRow, false, true);
    // Called directly rather than left to selectedRowsChanged: JUCE's ListBox does not renotify
    // when the row *index* is unchanged even though the array underneath was just rebuilt, so the
    // details could be left showing a row that has moved.
    selectionChanged (newRow);

    statusLabel.setText (juce::String (rows.size()) + " preset" + (rows.size() == 1 ? "" : "s")
                             + " in " + library.getPresetDir().getFullPathName(),
                         juce::dontSendNotification);
}

void PresetPanel::selectionChanged (int row)
{
    // Whatever was previewing was previewing the row being left.
    stopAuditionIfRunning();

    selectedRow = row;
    haveSelection = juce::isPositiveAndBelow (row, (int) rows.size());
    if (haveSelection)
        selectedCopy = *rows[(size_t) row];

    updateDetailsFromSelection();
    updateEnablement();

    if (onSelectionChanged)
        onSelectionChanged (haveSelection ? &selectedCopy : nullptr);

    // Selecting a row loads it, unless it is already the open one -- which is what stops a refresh
    // re-selecting the same row from reloading the document on every unrelated edit.
    if (haveSelection && selectedCopy.key() != currentKey)
        loadEntry (selectedCopy);
}

void PresetPanel::updateDetailsFromSelection()
{
    if (haveSelection)
    {
        // From the file, not from the tree: they agree after a save, but only the file is right
        // after somebody renamed it in a file manager. A built-in has no file, so its resource name
        // stands in -- and Save As is how it becomes a file of that name.
        nameEditor.setText (selectedCopy.isBuiltIn ? selectedCopy.meta.name
                                                   : selectedCopy.file.getFileNameWithoutExtension(),
                            false);
        authorEditor.setText (selectedCopy.meta.author, false);
        commentEditor.setText (selectedCopy.meta.comment, false);
        descriptionEditor.setText (selectedCopy.meta.description, false);
        tagsEditor.setText (selectedCopy.meta.tags.joinIntoString (", "), false);
        dateLabel.setText ("saved " + selectedCopy.meta.date, juce::dontSendNotification);
    }
    else
    {
        // Nothing selected: default from the live document, so a never-saved one still has
        // sensible Save As defaults.
        const auto doc = host.captureDocument();
        nameEditor.setText (doc.getProperty (format.nameProperty, "").toString(), false);
        authorEditor.setText (doc.getProperty (format.authorProperty, "").toString(), false);
        commentEditor.setText ({}, false);
        descriptionEditor.setText ({}, false);
        tagsEditor.setText ({}, false);
        dateLabel.setText ({}, juce::dontSendNotification);
    }
}

juce::String PresetPanel::typedBaseName() const
{
    return PresetFile::sanitiseFilename (nameEditor.getText().trim());
}

bool PresetPanel::nameMatchesCurrentFile() const
{
    return currentFile != juce::File()
           && typedBaseName() == currentFile.getFileNameWithoutExtension();
}

void PresetPanel::updateEnablement()
{
    // Save means "write back to the file this document is open from". Once the name field names
    // some other file, that is no longer what is being asked for, so Save As is the way out.
    const bool canSave = nameMatchesCurrentFile();
    saveButton.setEnabled (canSave);
    saveButton.setTooltip (canSave
        ? "write the live document back to " + currentFile.getFileName()
        : (currentFile == juce::File() ? "this document has no file yet -- use Save As"
                                       : "the name field no longer names the open file -- use Save "
                                         "As to write it under the new name, or rename to move it"));

    // A built-in is readable, loadable and duplicable, but never writable.
    const bool writable = selectionIsWritable();
    renameButton.setEnabled (writable);
    deleteButton.setEnabled (writable);
    if (haveSelection && ! writable)
    {
        const auto why = "built-in -- duplicate it to get an editable copy";
        renameButton.setTooltip (why);
        deleteButton.setTooltip (why);
    }
    else
    {
        renameButton.setTooltip ({});
        deleteButton.setTooltip ({});
    }

    auditionButton.setEnabled (auditioner != nullptr && haveSelection
                               && auditioner->canAudition (selectedCopy));
}

void PresetPanel::loadEntry (const PresetEntry& entry)
{
    const auto label = entry.isBuiltIn ? entry.meta.name : entry.file.getFileNameWithoutExtension();

    auto loaded = entry.isBuiltIn
        ? PresetFile::loadFromData (entry.data, entry.dataSize, entry.meta.name, format)
        : PresetFile::load (entry.file, format);

    if (! loaded.has_value())
    {
        showError ("Could not load \"" + label + "\" -- it may be corrupt.");
        return;
    }

    host.applyDocument (loaded->tree);
    // A built-in has no file to write back to, so the open document has no on-disk identity and
    // Save falls through to Save As -- which is exactly what editing a built-in should do.
    currentFile = entry.file;
    currentKey = entry.key();
    host.documentLoaded (entry.file, loaded->meta);

    statusLabel.setText ("loaded \"" + label + "\"", juce::dontSendNotification);
    updateEnablement();   // the open file changed, and with it what Save means
}

PresetMetadata PresetPanel::metadataFromEditors() const
{
    PresetMetadata m;
    // The name is the file's; this is what has been typed for it, sanitised the way a filename is.
    m.name = typedBaseName();
    m.author = authorEditor.getText().trim();
    m.comment = commentEditor.getText().trim();
    m.description = descriptionEditor.getText();
    m.tags = juce::StringArray::fromTokens (tagsEditor.getText(), ",", "");
    m.tags.trim();
    m.tags.removeEmptyStrings();
    m.date = juce::Time::getCurrentTime().toISO8601 (true);
    // `extra` is deliberately left alone here: it belongs to a *document*, not to these editors,
    // and saving and renaming disagree about which document that is. Each fills it in itself.
    return m;
}

void PresetPanel::writeTo (const juce::File& file)
{
    auto meta = metadataFromEditors();
    // A save writes the *live* document, so its listable extras are the live one's.
    meta.extra = host.captureExtraMetadata();

    if (! PresetFile::save (file, host.captureDocument(), meta, format))
    {
        showError ("Could not write \"" + file.getFullPathName() + "\".");
        return;
    }

    auto saved = meta;
    saved.name = file.getFileNameWithoutExtension();   // the file had the last word
    host.documentSaved (file, saved);

    currentFile = file;
    currentKey = file.getFullPathName();
    statusLabel.setText ("saved \"" + saved.name + "\"", juce::dontSendNotification);
    rescanAndRefreshList();
}

void PresetPanel::doNew()
{
    // Nothing is open afterwards, so nothing may be previewing either.
    stopAuditionIfRunning();
    host.newDocument();

    // No file and no key: `updateEnablement` reads that as "this document has no file yet", which
    // turns Save off and points at Save As -- an empty preset that cannot be written over anything
    // is exactly what New is for.
    currentFile = juce::File();
    currentKey = {};

    // Deselect *before* the details are refilled: with no row selected they default from the live
    // document, which `newDocument()` has just emptied, so every field clears with nothing here
    // having to name them one by one. `deselectAllRows` does not call back into the model, hence
    // the state kept beside it is cleared by hand.
    list.deselectAllRows();
    selectedRow = -1;
    haveSelection = false;
    selectedCopy = {};
    if (onSelectionChanged) onSelectionChanged (nullptr);

    updateDetailsFromSelection();
    updateEnablement();
    statusLabel.setText ("new preset", juce::dontSendNotification);
}

void PresetPanel::doSave()
{
    // Disabled in that case, but a keyboard shortcut or a stale click must not write the live
    // document over a file the name field no longer names.
    if (! nameMatchesCurrentFile()) { doSaveAs(); return; }
    writeTo (currentFile);
}

void PresetPanel::doSaveAs()
{
    auto folder = host.presetFolder();
    folder.createDirectory();

    // No file chooser: the folder is this panel's own setting and the name is the field above the
    // button, so a native dialog would ask for both again. The document's own name is the fallback
    // for an empty field.
    auto base = typedBaseName();
    if (nameEditor.getText().trim().isEmpty())
        base = PresetFile::sanitiseFilename (
            host.captureDocument().getProperty (format.nameProperty, "").toString());

    const auto target = folder.getChildFile (base + format.extension);

    if (! target.existsAsFile() || target == currentFile)
    {
        writeTo (target);
        return;
    }

    // Replacing a file is what a native save dialog's warnAboutOverwriting used to ask about.
    auto opts = juce::MessageBoxOptions::makeOptionsOkCancel (
        juce::MessageBoxIconType::WarningIcon, "Replace Preset",
        "\"" + target.getFileName() + "\" already exists in this folder. Replace it?",
        "Replace", "Cancel", this);

    juce::AlertWindow::showAsync (opts,
        [safe = juce::Component::SafePointer<PresetPanel> (this), target] (int result)
    {
        if (auto* o = safe.getComponent(); o != nullptr && result == 1)
            o->writeTo (target);
    });
}

void PresetPanel::doDelete()
{
    // Disabled for a built-in, but a keyboard route or a stale click must not get past it either.
    if (! selectionIsWritable()) return;
    const auto copy = selectedCopy;

    auto opts = juce::MessageBoxOptions::makeOptionsOkCancel (
        juce::MessageBoxIconType::WarningIcon, "Delete Preset",
        "Delete \"" + copy.file.getFileName() + "\"? This moves the file to the trash.",
        "Delete", "Cancel", this);

    // SafePointer, never a raw `this`: closing the plugin window while a confirmation is up must
    // not call back into a destroyed panel.
    juce::AlertWindow::showAsync (opts,
        [safe = juce::Component::SafePointer<PresetPanel> (this), copy] (int result)
    {
        auto* o = safe.getComponent();
        if (o == nullptr || result != 1) return;

        juce::String err;
        if (! o->library.deletePreset (copy, err))
        {
            o->showError (err);
            return;
        }

        if (copy.key() == o->currentKey)
        {
            o->currentFile = juce::File();   // its on-disk identity is gone; next Save is a Save As
            o->currentKey = {};
        }

        o->haveSelection = false;
        o->selectedRow = -1;
        o->rescanAndRefreshList();
    });
}

void PresetPanel::doRename()
{
    if (! selectionIsWritable()) return;

    const auto oldFile = selectedCopy.file;

    auto meta = metadataFromEditors();
    // A rename edits a file that already exists, which may not be the open document -- so its
    // extras stay the ones it was saved with rather than being restamped from the live document.
    meta.extra = selectedCopy.meta.extra;

    juce::String err;
    if (! library.renamePreset (selectedCopy, meta, err))
    {
        showError (err);
        return;
    }

    // If this was the file the live document came from, keep it and this panel in step -- or the
    // next plain Save would write the old name straight back over the rename.
    if (oldFile != juce::File() && oldFile == currentFile)
    {
        currentFile = selectedCopy.file;
        currentKey = selectedCopy.key();
        host.documentSaved (selectedCopy.file, selectedCopy.meta);
    }

    statusLabel.setText ("renamed to \"" + selectedCopy.file.getFileName() + "\"",
                         juce::dontSendNotification);
    rescanAndRefreshList();
    updateEnablement();
}

void PresetPanel::doDuplicate()
{
    if (! haveSelection) return;

    juce::File copy;
    juce::String err;
    if (! library.duplicatePreset (selectedCopy, copy, err))
    {
        showError (err);
        return;
    }

    statusLabel.setText ("copied to \"" + copy.getFileName() + "\"", juce::dontSendNotification);
    rescanAndRefreshList();
}

void PresetPanel::showRowMenu (int row)
{
    if (! juce::isPositiveAndBelow (row, (int) rows.size())) return;

    // Acting on the row under the pointer means selecting it first -- and selecting loads it, the
    // same as a left click.
    if (row != selectedRow)
    {
        list.selectRow (row);
        selectionChanged (row);
    }
    if (! haveSelection) return;

    const bool writable = selectionIsWritable();

    juce::PopupMenu m;
    m.addItem (1, "show in file manager", writable);
    m.addItem (2, "rename...", writable);
    // Enabled for a built-in on purpose: duplicating is how one becomes editable.
    m.addItem (3, writable ? "duplicate" : "duplicate to my presets");
    m.addSeparator();
    m.addItem (4, "delete...", writable);

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&list),
                     [safe = juce::Component::SafePointer<PresetPanel> (this)] (int result)
    {
        auto* o = safe.getComponent();
        if (o == nullptr || result == 0 || ! o->haveSelection) return;

        switch (result)
        {
            case 1: o->selectedCopy.file.revealToUser(); break;
            // The name field *is* the rename control, so this puts the caret in it rather than
            // offering a second way to type the same thing.
            case 2: o->nameEditor.grabKeyboardFocus();
                    o->nameEditor.selectAll();
                    break;
            case 3: o->doDuplicate(); break;
            case 4: o->doDelete();    break;
            default: break;
        }
    });
}

void PresetPanel::showError (const juce::String& message)
{
    auto opts = juce::MessageBoxOptions::makeOptionsOk (
        juce::MessageBoxIconType::WarningIcon, "Preset Librarian", message, "OK", this);
    juce::AlertWindow::showAsync (opts, nullptr);   // nothing captured, nothing unsafe
}

void PresetPanel::refresh()
{
    applyFilterAndRefreshList();
    refreshFolderLabel();   // the folder can be changed from here, but it is also a plugin setting
}

void PresetPanel::paint (juce::Graphics& g)
{
    g.setColour (colours.background);
    g.fillAll();
}

void PresetPanel::resized()
{
    auto b = getLocalBounds().reduced (6);

    auto details = b.removeFromRight (juce::jmax (220, b.getWidth() / 3));
    b.removeFromRight (6);
    auto listCol = b;

    // ── list column ──────────────────────────────────────────────────────────
    auto top = listCol.removeFromTop (24);
    refreshButton.setBounds (top.removeFromRight (56));
    top.removeFromRight (4);
    tagFilterBox.setBounds (top.removeFromRight (110));
    top.removeFromRight (4);
    searchBox.setBounds (top);

    if (builtInToggle.isVisible())
    {
        auto sources = listCol.removeFromTop (20);
        builtInToggle.setBounds (sources.removeFromLeft (90));
        userToggle.setBounds (sources.removeFromLeft (70));
    }

    if (extras.extraFilters != nullptr && extras.extraFiltersHeight > 0)
        extras.extraFilters->setBounds (listCol.removeFromTop (extras.extraFiltersHeight));

    listCol.removeFromTop (2);
    statusLabel.setBounds (listCol.removeFromBottom (16));

    auto folderRow = listCol.removeFromBottom (20);
    folderButton.setBounds (folderRow.removeFromLeft (66).reduced (2, 1));
    folderLabel.setBounds (folderRow);

    list.setBounds (listCol.reduced (0, 2));

    // ── details column ───────────────────────────────────────────────────────
    auto r = details;

    if (extras.detailsHeader != nullptr && extras.detailsHeaderHeight > 0)
    {
        extras.detailsHeader->setBounds (r.removeFromTop (extras.detailsHeaderHeight));
        r.removeFromTop (4);
    }
    if (auditionButton.isVisible())
    {
        auditionButton.setBounds (r.removeFromTop (22).reduced (2, 0));
        r.removeFromTop (4);
    }

    nameLabel.setBounds (r.removeFromTop (14));
    {
        // The extension sits inside the name row, dimmed: part of the filename, not part of what
        // can be typed.
        auto nameRow = r.removeFromTop (22);
        const int extWidth = juce::jmin (nameRow.getWidth() / 3, 74);
        extensionLabel.setBounds (nameRow.removeFromRight (extWidth).withTrimmedLeft (3));
        nameEditor.setBounds (nameRow);
    }
    r.removeFromTop (4);
    authorLabel.setBounds (r.removeFromTop (14));
    authorEditor.setBounds (r.removeFromTop (22));
    r.removeFromTop (4);
    commentLabel.setBounds (r.removeFromTop (14));
    commentEditor.setBounds (r.removeFromTop (22));
    r.removeFromTop (4);
    tagsLabel.setBounds (r.removeFromTop (14));
    tagsEditor.setBounds (r.removeFromTop (22));
    r.removeFromTop (4);
    descriptionLabel.setBounds (r.removeFromTop (14));
    descriptionEditor.setBounds (r.removeFromTop (56));
    r.removeFromTop (4);
    dateLabel.setBounds (r.removeFromTop (14));
    r.removeFromTop (8);

    // New sits with the two saves: the three things that decide what the live document *is*.
    auto row1 = r.removeFromTop (24);
    const int third = row1.getWidth() / 3;
    newButton.setBounds (row1.removeFromLeft (third).reduced (2, 0));
    saveButton.setBounds (row1.removeFromLeft (third).reduced (2, 0));
    saveAsButton.setBounds (row1.reduced (2, 0));
    r.removeFromTop (4);
    auto row2 = r.removeFromTop (24);
    renameButton.setBounds (row2.removeFromLeft (row2.getWidth() / 2).reduced (2, 0));
    deleteButton.setBounds (row2.reduced (2, 0));
}

} // namespace atl
