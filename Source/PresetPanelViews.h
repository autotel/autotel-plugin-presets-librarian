#pragma once
#include "PresetLibrary.h"
#include "PresetPanelStyle.h"

/*  The pieces `atl::PresetPanel` is assembled from. Each one draws and reports; none of them decides
    anything -- what a click *means* is the panel's, which is why they talk to it through callbacks
    rather than reaching into it. Public so a test can find them among the panel's children. */

namespace atl
{

/** What a dragged preset row carries: this prefix, then the row's `PresetEntry::key()`. */
extern const juce::String presetDragPrefix;

// ── tag rail ────────────────────────────────────────────────────────────────

/** Every tag in the library with a count -- the answer to "what tags do I even have". Clicking one
    toggles it as a filter; dropping a dragged preset on one assigns it. */
class TagRail : public juce::Component
{
public:
    TagRail();
    ~TagRail() override;

    void setPalette (const PanelPalette&);
    void setTags (std::vector<TagCount>, const juce::StringArray& active);

    std::function<void (const juce::String& tag)> onTagClicked;
    std::function<void (const juce::String& tag, const juce::String& presetKey)> onPresetDropped;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int width = 176;

private:
    struct Content;
    PanelPalette pal = PanelPalette::from ({});
    juce::Viewport viewport;
    std::unique_ptr<Content> content;
};

// ── active filter bar ───────────────────────────────────────────────────────

/** Shown only while tags are filtering: each as a removable chip, `match all` / `match any`, and
    `clear`. */
class ActiveFilterBar : public juce::Component
{
public:
    ActiveFilterBar();

    void setPalette (const PanelPalette&);
    void setTags (const juce::StringArray& tags, bool matchAny);

    std::function<void (const juce::String&)> onRemoveTag;
    std::function<void()> onToggleMode, onClear;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int height = 34;

    juce::TextButton modeButton, clearButton { "clear" };

private:
    PanelPalette pal = PanelPalette::from ({});
    juce::OwnedArray<juce::TextButton> chips;
};

// ── live strip ──────────────────────────────────────────────────────────────

/** The pinned live patch: what is about to be lost, without having to find a row for it. Sits above
    every row and is outside sort and filter. */
class LiveStrip : public juce::Component
{
public:
    struct State
    {
        juce::String name;       // empty: never saved
        juce::String meta;       // "author · date", or empty
        bool dirty = false;
        bool canSave = false, saveIsAccent = false, canRevert = false;
    };

    LiveStrip();

    void setPalette (const PanelPalette&);
    void setState (const State&);
    const State& getState() const { return state; }

    std::function<void()> onEdit, onSave, onRevert;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int height = 42;

    juce::TextButton editButton, saveButton { "save" }, revertButton { "revert to disk" };

private:
    PanelPalette pal = PanelPalette::from ({});
    State state;
    juce::Rectangle<int> textArea;
};

// ── preset list ─────────────────────────────────────────────────────────────

/** The rows. Painted rather than built from a component per row, so a folder of thousands scrolls
    as cheaply as a folder of ten; the only real components are the selected row's action strip and
    the edit popover, which is borrowed from the panel and laid out *in* the list, pushing the rows
    under it down.

    Single click selects, and never loads. Double-click and Return load, and so does the `load` chip
    every row carries at its right edge -- one click, for going through presets one after another. */
class PresetListView : public juce::Component
{
public:
    /** Everything the list asks of whoever owns it. */
    struct Model
    {
        virtual ~Model() = default;

        virtual int getNumRows() = 0;
        virtual const PresetEntry* getRow (int) = 0;
        virtual bool isRowOpen (const PresetEntry&) = 0;
        virtual bool isRowFavourite (const PresetEntry&) = 0;
        virtual bool isTagFiltering (const juce::String&) = 0;
        virtual bool showComments() = 0;
        virtual bool canAudition (const PresetEntry&) = 0;
        virtual bool hasAuditioner() = 0;

        virtual void rowSelected (int row) = 0;   // -1: selection cleared
        virtual void rowLoadRequested (int row) = 0;
        virtual void rowEditRequested (int row) = 0;
        virtual void rowDuplicateRequested (int row) = 0;
        virtual void rowDeleteRequested (int row) = 0;
        virtual void rowAuditionRequested (int row) = 0;
        virtual void rowRevealRequested (int row) = 0;
        virtual void rowFavouriteToggled (int row) = 0;
        virtual void rowTagClicked (const juce::String& tag) = 0;
        virtual void rowDragStarted (int row) = 0;
        virtual void escapePressed() = 0;
    };

    PresetListView();
    ~PresetListView() override;

    void setModel (Model* m) { model = m; }
    void setPalette (const PanelPalette&);

    /** The edit popover the list lays out. Borrowed from the panel. */
    void setPopover (juce::Component* popover, std::function<int (int width)> heightForWidth);

    /** Re-reads every row from the model and re-lays out. */
    void updateContent();

    /** Selects without asking the model's permission; `notify` says whether `rowSelected` fires. */
    void selectRow (int row, bool notify = true);
    int getSelectedRow() const { return selectedRow; }
    int getNumRows() const { return numRows; }

    /** Where the popover sits: under `row`, under the live strip (`showPopoverForLive`), or nowhere. */
    void showPopoverForRow (int row);
    void showPopoverForLive();
    void hidePopover();
    bool isPopoverShowing() const { return popoverRow >= 0 || popoverLive; }
    int getPopoverRow() const { return popoverRow; }

    /** Re-lays out without re-reading the rows -- the popover's height changed. */
    void relayout();

    void setDraggingRow (int row);
    void scrollToRow (int row);
    void scrollToTop();

    /** The row's one-click `load` chip, in the list's scrolled content's coordinates. */
    juce::Rectangle<int> loadAreaForRow (int row) const;

    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void focusGained (FocusChangeType) override { repaint(); }

    juce::TextButton loadButton { "load" }, auditionButton { "audition" }, editButton,
                     duplicateButton { "duplicate" }, deleteButton { "delete" };

private:
    struct Content;
    friend struct Content;

    struct RowGeometry
    {
        juce::Rectangle<int> star, name, comment, meta, load;
        std::vector<std::pair<juce::Rectangle<int>, juce::String>> tags;
        juce::Rectangle<int> factoryBadge, openBadge;
    };

    int baseRowHeight (const PresetEntry&) const;
    RowGeometry geometryFor (const PresetEntry&, juce::Rectangle<int> row) const;
    juce::Rectangle<int> rowBounds (int row) const;
    int rowAt (int contentY) const;
    void paintRows (juce::Graphics&, juce::Rectangle<int> clip);
    void showRowMenu (int row);
    void layoutActionStrip();
    void updateActionButtons();

    Model* model = nullptr;
    PanelPalette pal = PanelPalette::from ({});
    juce::Viewport viewport;
    std::unique_ptr<Content> content;

    juce::Component* popover = nullptr;
    std::function<int (int)> popoverHeight;

    int numRows = 0;
    std::vector<int> rowTops, rowHeights;   // rowHeights excludes the popover and the divider
    int selectedRow = -1, popoverRow = -1, hoverRow = -1, draggingRow = -1;
    bool popoverLive = false, hoverLoad = false;
    int emptyTop = 0;
    juce::Rectangle<int> hintArea;
    juce::String hint;
};

// ── modal ───────────────────────────────────────────────────────────────────

/** A dialog drawn inside the panel: a scrim over everything, a card in the middle. Inside rather
    than a native window, so it sits in the plugin's own skin, closes with the editor, and a test
    can press its buttons without a message loop.

    Built from parts, top to bottom, then buttons: left-aligned ones in the order added, and at most
    one trailing `quiet` one (cancel) pushed to the right. */
class PresetModal : public juce::Component
{
public:
    PresetModal (const PanelPalette&, int cardWidth);

    void setTitle (const juce::String&);
    void addText (const juce::String&);
    void addCaption (const juce::String&);
    void addPathBox (const juce::String&);
    /** A boxed paragraph whose text can be changed later -- the migrate summary. */
    juce::Label& addSummary (const juce::String&);
    juce::TextEditor& addPathInput (const juce::String& placeholder, juce::TextButton*& browseOut);

    juce::TextButton& addButton (const juce::String& text, ButtonStyle, std::function<void()>);
    /** The trailing, right-aligned one. Also what Escape presses. */
    juce::TextButton& addCancel (const juce::String& text, std::function<void()>);

    /** Sizes the card to its parts and centres it in this component. */
    void resized() override;
    void paint (juce::Graphics&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override {}   // the scrim swallows clicks

    juce::TextButton* findButton (const juce::String& text) const;

private:
    enum class Kind { text, caption, pathBox, summary, pathInput };
    struct Part
    {
        Kind kind;
        juce::String text;
        std::unique_ptr<juce::Component> component;
        std::unique_ptr<juce::TextButton> browse;
        juce::Rectangle<int> area;
    };

    int heightOfPart (Part&, int width) const;

    PanelPalette pal;
    int cardWidth;
    juce::String title;
    std::vector<std::unique_ptr<Part>> parts;
    juce::OwnedArray<juce::TextButton> buttons;
    std::unique_ptr<juce::TextButton> cancel;
    std::function<void()> cancelAction;
    juce::Rectangle<int> card;
};

} // namespace atl
