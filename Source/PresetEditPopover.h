#pragma once
#include "PresetFile.h"
#include "PresetPanelStyle.h"

namespace atl
{

/** The precise path for tags: the ones a preset carries as removable chips, an input that adds or
    creates one on Return or a comma, and the library's other tags underneath as dashed suggestions, narrowed
    as you type. */
class TagPicker : public juce::Component
{
public:
    TagPicker();

    void setPalette (const PanelPalette&);

    /** `tags` are the preset's own; `vocabulary` every tag in the library, for suggestions and so
        that typing "Bell" joins an existing "bell" rather than starting a second spelling. */
    void setTags (const juce::StringArray& tags, const juce::StringArray& vocabulary);
    juce::StringArray getTags() const { return tags; }

    /** Adds `typed` -- trimmed, split on commas into one tag per piece (tags are stored
        comma-joined), and spelt like an existing tag when one matches ignoring case. Empty or
        already-present is a no-op. */
    void addTag (const juce::String& typed);
    void removeTag (const juce::String&);

    int getHeightForWidth (int width);
    void resized() override;

    /** The height needed has changed (a chip came or went, suggestions narrowed). */
    std::function<void()> onLayoutChanged;

    juce::TextEditor input;

private:
    void rebuild();
    int layout (int width, bool apply);

    PanelPalette pal = PanelPalette::from ({});
    juce::StringArray tags, vocabulary;
    juce::OwnedArray<juce::TextButton> chips, suggestions;
};

/** The metadata fields, opened *at* the preset they describe: under its row, or under the pinned
    live strip. One exists per panel and moves to wherever it is opened.

    It edits a draft only. Nothing reaches a file until `onApply`, and what apply then means --
    rename, write metadata, write a copy, save as -- is the panel's call, which the footer's hint
    states before the click rather than after. */
class PresetEditPopover : public juce::Component
{
public:
    /** What `apply` will do, which is what the footer hint says. */
    enum class Mode
    {
        edit,          // a writable file: metadata written, a changed name renames it
        factoryCopy,   // a built-in: apply writes an editable copy into the preset folder
        liveUnsaved,   // the live patch has no file yet: apply names it, save writes it
        saveAs         // save as: apply writes the live patch to a new file
    };

    explicit PresetEditPopover (const juce::String& extension);

    void setPalette (const PanelPalette&);

    /** `heading` is the caps label (`editing`, `editing live patch`); `title` the preset's name as
        it stands. `live` tints the heading with the accent, which is reserved for the live patch. */
    void open (const juce::String& heading, const juce::String& title, const PresetMetadata& meta,
               const juce::StringArray& vocabulary, Mode mode, bool live);

    Mode getMode() const { return mode; }

    /** What the fields say now. `name` is as typed (trimmed, not yet sanitised). */
    PresetMetadata getDraft() const;

    /** A plugin's `PresetPanelExtras::detailsHeader`: sits above `file name`. Borrowed. */
    void setHeader (juce::Component*, int height);

    void focusName();
    int getHeightForWidth (int width);

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    std::function<void()> onApply, onCancel, onLayoutChanged;

    juce::TextEditor nameEditor, authorEditor, commentEditor, descriptionEditor;
    TagPicker tagPicker;
    juce::TextButton applyButton { "apply" }, cancelButton { "cancel" },
                     closeButton { juce::String (juce::CharPointer_UTF8 ("\xc3\x97")) };

private:
    int layout (int width, bool apply);

    PanelPalette pal = PanelPalette::from ({});
    juce::String extension, heading, title;
    Mode mode = Mode::edit;
    bool live = false;
    PresetMetadata original;

    juce::Component* header = nullptr;
    int headerHeight = 0;

    struct Caption { juce::Rectangle<int> area; juce::String text; };
    std::vector<Caption> captions;
    juce::Rectangle<int> headingArea, extensionArea, hintArea;
    int dividerY = 0;
};

} // namespace atl
