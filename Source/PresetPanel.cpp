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

    const bool isOpen = (e.file == owner.currentFile);
    g.setColour (isOpen ? c.accent : c.text);
    g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    // The filename is the name (see PresetMetadata), so it is what the row says.
    g.drawText ((isOpen ? juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb8 ")) : juce::String())
                    + e.file.getFileNameWithoutExtension(),
                8, 1, width - 12, height / 2, juce::Justification::centredLeft, true);

    juce::StringArray subParts;
    // First, because it is the note the user left themselves about *this* file.
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

PresetPanel::~PresetPanel() = default;

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

    const auto previousKey = haveSelection ? selectedCopy.key() : juce::String();

    rows = library.getFiltered (filter);
    list.updateContent();

    int newRow = -1;
    if (previousKey.isNotEmpty())
    {
        for (size_t i = 0; i < rows.size(); ++i)
            if (rows[i]->key() == previousKey) { newRow = (int) i; break; }
    }
    else if (currentFile != juce::File())
    {
        for (size_t i = 0; i < rows.size(); ++i)
            if (rows[i]->file == currentFile) { newRow = (int) i; break; }
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
    selectedRow = row;
    haveSelection = juce::isPositiveAndBelow (row, (int) rows.size());
    if (haveSelection)
        selectedCopy = *rows[(size_t) row];

    updateDetailsFromSelection();
    updateEnablement();

    // Selecting a row loads it, unless it is already the open one -- which is what stops a refresh
    // re-selecting the same row from reloading the document on every unrelated edit.
    if (haveSelection && selectedCopy.file != currentFile)
        loadEntry (selectedCopy);
}

void PresetPanel::updateDetailsFromSelection()
{
    if (haveSelection)
    {
        // From the file, not from the tree: they agree after a save, but only the file is right
        // after somebody renamed it in a file manager.
        nameEditor.setText (selectedCopy.file.getFileNameWithoutExtension(), false);
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
    renameButton.setEnabled (haveSelection);
    deleteButton.setEnabled (haveSelection);
}

void PresetPanel::loadEntry (const PresetEntry& entry)
{
    auto loaded = PresetFile::load (entry.file, format);
    if (! loaded.has_value())
    {
        showError ("Could not load \"" + entry.file.getFileName() + "\" -- it may be corrupt.");
        return;
    }

    host.applyDocument (loaded->tree);
    currentFile = entry.file;

    statusLabel.setText ("loaded \"" + entry.file.getFileNameWithoutExtension() + "\"",
                         juce::dontSendNotification);
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
    return m;
}

void PresetPanel::writeTo (const juce::File& file)
{
    const auto meta = metadataFromEditors();
    if (! PresetFile::save (file, host.captureDocument(), meta, format))
    {
        showError ("Could not write \"" + file.getFullPathName() + "\".");
        return;
    }

    auto saved = meta;
    saved.name = file.getFileNameWithoutExtension();   // the file had the last word
    host.documentSaved (file, saved);

    currentFile = file;
    statusLabel.setText ("saved \"" + saved.name + "\"", juce::dontSendNotification);
    rescanAndRefreshList();
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
    if (! haveSelection) return;
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

        if (copy.file == o->currentFile)
            o->currentFile = juce::File();   // its on-disk identity is gone; next Save is a Save As

        o->haveSelection = false;
        o->selectedRow = -1;
        o->rescanAndRefreshList();
    });
}

void PresetPanel::doRename()
{
    if (! haveSelection) return;

    const auto oldFile = selectedCopy.file;

    juce::String err;
    if (! library.renamePreset (selectedCopy, metadataFromEditors(), err))
    {
        showError (err);
        return;
    }

    // If this was the file the live document came from, keep it and this panel in step -- or the
    // next plain Save would write the old name straight back over the rename.
    if (oldFile == currentFile)
    {
        currentFile = selectedCopy.file;
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

    juce::PopupMenu m;
    m.addItem (1, "show in file manager");
    m.addItem (2, "rename...");
    m.addItem (3, "duplicate");
    m.addSeparator();
    m.addItem (4, "delete...");

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

    listCol.removeFromTop (2);
    statusLabel.setBounds (listCol.removeFromBottom (16));

    auto folderRow = listCol.removeFromBottom (20);
    folderButton.setBounds (folderRow.removeFromLeft (66).reduced (2, 1));
    folderLabel.setBounds (folderRow);

    list.setBounds (listCol.reduced (0, 2));

    // ── details column ───────────────────────────────────────────────────────
    auto r = details;
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

    auto row1 = r.removeFromTop (24);
    saveButton.setBounds (row1.removeFromLeft (row1.getWidth() / 2).reduced (2, 0));
    saveAsButton.setBounds (row1.reduced (2, 0));
    r.removeFromTop (4);
    auto row2 = r.removeFromTop (24);
    renameButton.setBounds (row2.removeFromLeft (row2.getWidth() / 2).reduced (2, 0));
    deleteButton.setBounds (row2.reduced (2, 0));
}

} // namespace atl
