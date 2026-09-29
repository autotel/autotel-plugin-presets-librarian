#include "PresetPanelViews.h"

namespace atl
{

const juce::String presetDragPrefix { "atl-preset:" };

namespace
{
const juce::String ellipsis (juce::CharPointer_UTF8 ("\xe2\x80\xa6"));
const juce::String middot   (juce::CharPointer_UTF8 ("\xc2\xb7"));
const juce::String times    (juce::CharPointer_UTF8 ("\xc3\x97"));

int textWidth (const juce::Font& f, const juce::String& s)
{
    return (int) std::ceil (PanelFonts::width (f, s));
}

/** Draws the caps label and returns how wide it was. */
int drawCaps (juce::Graphics& g, const juce::String& text, int x, int y, int h, float cssPx)
{
    const auto f = PanelFonts::caps (cssPx);
    const auto upper = text.toUpperCase();
    g.setFont (f);
    const int w = textWidth (f, upper) + 2;
    g.drawText (upper, x, y, w, h, juce::Justification::centredLeft, false);
    return w;
}
} // namespace

// ── tag rail ────────────────────────────────────────────────────────────────

struct TagRail::Content : public juce::Component,
                          public juce::DragAndDropTarget,
                          public juce::SettableTooltipClient
{
    explicit Content (TagRail& o) : owner (o)
    {
        setTooltip (juce::String (juce::CharPointer_UTF8 ("click to filter \xc2\xb7 drop a preset here to tag it")));
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    static constexpr int headerH = 30, itemH = 26;

    int itemAt (int y) const
    {
        const int i = (y - headerH) / itemH;
        return y >= headerH && juce::isPositiveAndBelow (i, (int) tags.size()) ? i : -1;
    }

    int preferredHeight() const { return headerH + (int) tags.size() * itemH + 60; }

    void paint (juce::Graphics& g) override
    {
        const auto& p = owner.pal;
        g.setColour (p.textFaint);
        drawCaps (g, "tags", 12, 9, headerH - 12, 11.0f);

        for (int i = 0; i < (int) tags.size(); ++i)
        {
            const auto& t = tags[(size_t) i];
            const bool isActive = active.contains (t.tag, true);
            const bool hot = t.tag == dropTag;
            auto r = juce::Rectangle<int> (0, headerH + i * itemH, getWidth(), itemH);

            if (hot)                     g.setColour (p.accentFill.brighter (0.15f));
            else if (isActive)           g.setColour (p.accentFill.withMultipliedAlpha (0.7f));
            else if (i == hover)         g.setColour (p.rowHover);
            else                         g.setColour (juce::Colours::transparentBlack);
            g.fillRect (r);

            if (hot || isActive)
            {
                g.setColour (hot ? p.accentLight : p.base.accent);
                g.fillRect (r.removeFromLeft (2));
            }
            else r.removeFromLeft (2);

            r = r.withTrimmedLeft (10).withTrimmedRight (12);
            const auto countText = juce::String (t.count);
            const auto countFont = PanelFonts::mono (11.0f);
            g.setFont (countFont);
            g.setColour (p.textFaint);
            g.drawText (countText, r.removeFromRight (textWidth (countFont, countText) + 2),
                        juce::Justification::centredRight, false);

            g.setFont (PanelFonts::sans (12.5f));
            g.setColour (isActive ? p.accentText : p.base.text.withMultipliedAlpha (0.88f));
            g.drawText (t.tag, r.withTrimmedRight (8), juce::Justification::centredLeft, true);
        }

        const int hintY = headerH + (int) tags.size() * itemH + 8;
        g.setFont (PanelFonts::sans (11.0f));
        g.setColour (p.textFaint.withMultipliedAlpha (0.9f));
        g.drawFittedText (tags.empty() ? "no tags yet -- add them from a preset's edit" + ellipsis
                                       : juce::String ("drag a preset onto a tag to assign it"),
                          12, hintY, getWidth() - 24, 40, juce::Justification::topLeft, 3, 1.0f);
    }

    void mouseMove (const juce::MouseEvent& e) override { setHover (itemAt (e.y)); }
    void mouseExit (const juce::MouseEvent&) override  { setHover (-1); }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.mouseWasDraggedSinceMouseDown()) return;
        const int i = itemAt (e.y);
        if (i >= 0 && owner.onTagClicked) owner.onTagClicked (tags[(size_t) i].tag);
    }

    void setHover (int i) { if (i != hover) { hover = i; repaint(); } }

    bool isInterestedInDragSource (const SourceDetails& d) override
    {
        return d.description.toString().startsWith (presetDragPrefix);
    }

    void itemDragMove (const SourceDetails& d) override
    {
        const int i = itemAt (d.localPosition.y);
        const auto t = i >= 0 ? tags[(size_t) i].tag : juce::String();
        if (t != dropTag) { dropTag = t; repaint(); }
    }

    void itemDragExit (const SourceDetails&) override { dropTag = {}; repaint(); }

    void itemDropped (const SourceDetails& d) override
    {
        const int i = itemAt (d.localPosition.y);
        dropTag = {};
        repaint();
        if (i < 0 || owner.onPresetDropped == nullptr) return;
        owner.onPresetDropped (tags[(size_t) i].tag,
                               d.description.toString().fromFirstOccurrenceOf (presetDragPrefix, false, false));
    }

    TagRail& owner;
    std::vector<TagCount> tags;
    juce::StringArray active;
    juce::String dropTag;
    int hover = -1;
};

TagRail::TagRail() : content (std::make_unique<Content> (*this))
{
    viewport.setViewedComponent (content.get(), false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
}

TagRail::~TagRail() = default;

void TagRail::setPalette (const PanelPalette& p) { pal = p; repaint(); content->repaint(); }

void TagRail::setTags (std::vector<TagCount> tags, const juce::StringArray& active)
{
    content->tags = std::move (tags);
    content->active = active;
    resized();
    content->repaint();
}

void TagRail::paint (juce::Graphics& g)
{
    g.fillAll (pal.rail);
    g.setColour (pal.divider);
    g.fillRect (getWidth() - 1, 0, 1, getHeight());
}

void TagRail::resized()
{
    viewport.setBounds (getLocalBounds().withTrimmedRight (1));
    content->setSize (viewport.getMaximumVisibleWidth(),
                      juce::jmax (content->preferredHeight(), viewport.getHeight()));
}

// ── active filter bar ───────────────────────────────────────────────────────

ActiveFilterBar::ActiveFilterBar()
{
    setButtonStyle (modeButton, ButtonStyle::chip);
    modeButton.setTooltip ("match all selected tags, or any of them");
    modeButton.onClick = [this] { if (onToggleMode) onToggleMode(); };
    setButtonStyle (clearButton, ButtonStyle::quiet);
    clearButton.onClick = [this] { if (onClear) onClear(); };
    addAndMakeVisible (modeButton);
    addAndMakeVisible (clearButton);
}

void ActiveFilterBar::setPalette (const PanelPalette& p) { pal = p; repaint(); }

void ActiveFilterBar::setTags (const juce::StringArray& tags, bool matchAny)
{
    chips.clear();
    for (const auto& t : tags)
    {
        auto* b = chips.add (new juce::TextButton (t + "  " + times));
        setButtonStyle (*b, ButtonStyle::chipActive);
        b->setTooltip ("remove this tag filter");
        b->onClick = [this, t]
        {
            juce::Component::SafePointer<ActiveFilterBar> safe (this);
            juce::MessageManager::callAsync ([safe, t]
            {
                if (safe != nullptr && safe->onRemoveTag) safe->onRemoveTag (t);
            });
        };
        addAndMakeVisible (b);
    }
    modeButton.setButtonText (matchAny ? "match any" : "match all");
    resized();
}

void ActiveFilterBar::paint (juce::Graphics& g)
{
    g.fillAll (pal.rail);
    g.setColour (pal.divider);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);
    g.setFont (PanelFonts::sans (11.0f));
    g.setColour (pal.base.textDim);
    g.drawText ("filtering by", 12, 0, 80, getHeight() - 1, juce::Justification::centredLeft, false);
}

void ActiveFilterBar::resized()
{
    const auto font = PanelFonts::sans (11.5f);
    int x = 12 + textWidth (PanelFonts::sans (11.0f), "filtering by") + 8;
    const int h = 22, y = (getHeight() - 1 - h) / 2;
    for (auto* c : chips)
    {
        const int w = textWidth (font, c->getButtonText()) + 16;
        c->setBounds (x, y, w, h);
        x += w + 6;
    }
    const int mw = textWidth (font, modeButton.getButtonText()) + 18;
    modeButton.setBounds (x, y, mw, h);
    x += mw + 6;
    clearButton.setBounds (x, y, 44, h);
}

// ── live strip ──────────────────────────────────────────────────────────────

LiveStrip::LiveStrip()
{
    editButton.setButtonText ("edit" + ellipsis);
    editButton.setTooltip ("name, comment and tags of the live patch");
    revertButton.setTooltip ("re-read the open preset from disk");
    setButtonStyle (editButton, ButtonStyle::plain);
    setButtonStyle (saveButton, ButtonStyle::plain);
    setButtonStyle (revertButton, ButtonStyle::plain);
    editButton.onClick = [this] { if (onEdit) onEdit(); };
    saveButton.onClick = [this] { if (onSave) onSave(); };
    revertButton.onClick = [this] { if (onRevert) onRevert(); };
    for (auto* b : { &editButton, &saveButton, &revertButton })
        addAndMakeVisible (*b);
}

void LiveStrip::setPalette (const PanelPalette& p) { pal = p; repaint(); }

void LiveStrip::setState (const State& s)
{
    state = s;
    saveButton.setEnabled (s.canSave);
    setButtonStyle (saveButton, s.canSave && s.saveIsAccent ? ButtonStyle::accent : ButtonStyle::plain);
    revertButton.setEnabled (s.canRevert);
    repaint();
}

void LiveStrip::paint (juce::Graphics& g)
{
    g.fillAll (pal.liveStrip);
    g.setColour (pal.base.accent);
    g.fillRect (0, 0, 2, getHeight());
    g.setColour (pal.divider);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);

    auto r = textArea;
    const int h = r.getHeight();

    g.setColour (pal.accentLight);
    r.removeFromLeft (drawCaps (g, "live patch", r.getX(), r.getY(), h, 10.0f) + 9);

    // The chip is laid out first so the name gets whatever is left and ellipsises into it.
    const auto chipText = state.dirty ? juce::String (juce::CharPointer_UTF8 ("\xe2\x97\x8f unsaved changes"))
                                      : juce::String (juce::CharPointer_UTF8 ("\xe2\x97\x8b saved"));
    const auto chipFont = PanelFonts::sans (11.0f);
    const int chipW = textWidth (chipFont, chipText) + 20;

    const auto metaFont = PanelFonts::mono (11.0f);
    const int metaW = state.meta.isEmpty() ? 0 : textWidth (metaFont, state.meta) + 9;

    const auto nameFont = PanelFonts::sans (13.0f, true);
    const auto name = state.name.isEmpty() ? juce::String (juce::CharPointer_UTF8 ("new patch \xe2\x80\x94 never saved"))
                                           : state.name;
    const int nameW = juce::jmin (textWidth (nameFont, name) + 2,
                                  juce::jmax (40, r.getWidth() - chipW - metaW - 9));

    g.setFont (nameFont);
    g.setColour (pal.base.text);
    g.drawText (name, r.removeFromLeft (nameW), juce::Justification::centredLeft, true);
    r.removeFromLeft (9);

    if (metaW > 0)
    {
        g.setFont (metaFont);
        g.setColour (pal.base.textDim);
        g.drawText (state.meta, r.removeFromLeft (metaW - 9), juce::Justification::centredLeft, true);
        r.removeFromLeft (9);
    }

    if (r.getWidth() >= chipW)
    {
        const auto chip = r.removeFromLeft (chipW).withSizeKeepingCentre (chipW, 22).toFloat().reduced (0.5f);
        g.setColour (state.dirty ? pal.warningFill : pal.button.darker (0.1f));
        g.fillRoundedRectangle (chip, 11.0f);
        g.setColour (state.dirty ? pal.warningOutline : pal.base.outline);
        g.drawRoundedRectangle (chip, 11.0f, 1.0f);
        g.setFont (chipFont);
        g.setColour (state.dirty ? pal.base.warning : pal.base.textDim);
        g.drawText (chipText, chip, juce::Justification::centred, false);
    }
}

void LiveStrip::resized()
{
    auto r = getLocalBounds().withTrimmedBottom (1).withTrimmedLeft (2).reduced (10, 7);
    const auto f = PanelFonts::sans (12.0f);
    auto place = [&] (juce::TextButton& b)
    {
        b.setBounds (r.removeFromRight (textWidth (f, b.getButtonText()) + 22));
        r.removeFromRight (6);
    };
    place (revertButton);
    place (saveButton);
    place (editButton);
    r.removeFromRight (6);
    textArea = r;
}

// ── preset list ─────────────────────────────────────────────────────────────

namespace
{
constexpr int rowPadX = 12, rowPadY = 7, lineH = 20, commentH = 18, stripH = 40, indent = 27;
}

struct PresetListView::Content : public juce::Component
{
    explicit Content (PresetListView& o) : owner (o)
    {
        setWantsKeyboardFocus (false);
    }

    void paint (juce::Graphics& g) override { owner.paintRows (g, g.getClipBounds()); }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const int r = owner.rowAt (e.y);
        if (r != owner.hoverRow) { owner.hoverRow = r; repaint(); }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (owner.hoverRow != -1) { owner.hoverRow = -1; repaint(); }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (owner.isShowing()) owner.grabKeyboardFocus();
        pressedRow = -1;
        // A drag hands the mouse to the drag image, so this row may never see the mouseUp.
        dragging = false;
        const int row = owner.rowAt (e.y);
        if (row < 0 || owner.model == nullptr) return;
        const auto* entry = owner.model->getRow (row);
        if (entry == nullptr) return;

        const auto geo = owner.geometryFor (*entry, owner.rowBounds (row));
        if (geo.star.expanded (3).contains (e.getPosition()))
        {
            owner.model->rowFavouriteToggled (row);
            return;
        }
        for (const auto& [area, tag] : geo.tags)
            if (area.contains (e.getPosition()))
            {
                owner.model->rowTagClicked (tag);
                return;
            }

        if (row != owner.selectedRow) owner.selectRow (row);
        pressedRow = row;

        if (e.mods.isPopupMenu()) owner.showRowMenu (row);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (pressedRow < 0 || e.mods.isPopupMenu() || e.getDistanceFromDragStart() < 6 || dragging) return;
        const auto* entry = owner.model != nullptr ? owner.model->getRow (pressedRow) : nullptr;
        auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this);
        if (entry == nullptr || container == nullptr) return;

        dragging = true;
        owner.model->rowDragStarted (pressedRow);

        // The image is the name as a chip: what is being carried, not a ghost of the whole row.
        const auto name = displayName (*entry);
        const auto font = PanelFonts::sans (12.0f, true);
        const int w = textWidth (font, name) + 22, h = 24;
        juce::Image img (juce::Image::ARGB, w, h, true);
        {
            juce::Graphics ig (img);
            ig.setColour (owner.pal.card);
            ig.fillRoundedRectangle (0.5f, 0.5f, (float) w - 1, (float) h - 1, 11.0f);
            ig.setColour (owner.pal.base.accent);
            ig.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1, (float) h - 1, 11.0f, 1.0f);
            ig.setColour (owner.pal.base.text);
            ig.setFont (font);
            ig.drawText (name, 0, 0, w, h, juce::Justification::centred, false);
        }
        container->startDragging (presetDragPrefix + entry->key(), this, juce::ScaledImage (img), true,
                                  nullptr, &e.source);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        dragging = false;
        pressedRow = -1;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int row = owner.rowAt (e.y);
        // The first click of the pair selected it; guarded rather than assumed, because loading the
        // wrong preset is exactly the mistake a separate Load exists to prevent.
        if (row >= 0 && row == owner.selectedRow && owner.model != nullptr && ! e.mods.isPopupMenu())
        {
            const auto* entry = owner.model->getRow (row);
            if (entry == nullptr) return;
            const auto geo = owner.geometryFor (*entry, owner.rowBounds (row));
            if (geo.star.expanded (3).contains (e.getPosition())) return;
            for (const auto& t : geo.tags)
                if (t.first.contains (e.getPosition())) return;
            owner.model->rowLoadRequested (row);
        }
    }

    PresetListView& owner;
    int pressedRow = -1;
    bool dragging = false;
};

PresetListView::PresetListView() : content (std::make_unique<Content> (*this))
{
    setName ("preset list");
    setWantsKeyboardFocus (true);
    viewport.setViewedComponent (content.get(), false);
    viewport.setScrollBarsShown (true, false);
    viewport.setSingleStepSizes (16, 32);
    addAndMakeVisible (viewport);

    editButton.setButtonText ("edit" + ellipsis);
    setButtonStyle (loadButton, ButtonStyle::accent);
    for (auto* b : { &auditionButton, &editButton, &duplicateButton })
        setButtonStyle (*b, ButtonStyle::plain);
    setButtonStyle (deleteButton, ButtonStyle::danger);

    loadButton.setTooltip ("load this preset into the live patch -- selecting a row only shows it. "
                           "Double-click a row, or press Return, to do the same thing.");
    duplicateButton.setTooltip ("copy it under the next free name");

    auto act = [this] (void (Model::*fn) (int))
    {
        return [this, fn] { if (model != nullptr && selectedRow >= 0) (model->*fn) (selectedRow); };
    };
    loadButton.onClick      = act (&Model::rowLoadRequested);
    auditionButton.onClick  = act (&Model::rowAuditionRequested);
    editButton.onClick      = act (&Model::rowEditRequested);
    duplicateButton.onClick = act (&Model::rowDuplicateRequested);
    deleteButton.onClick    = act (&Model::rowDeleteRequested);

    for (auto* b : { &loadButton, &auditionButton, &editButton, &duplicateButton, &deleteButton })
        content->addChildComponent (*b);
}

PresetListView::~PresetListView()
{
    if (popover != nullptr && popover->getParentComponent() == content.get())
        content->removeChildComponent (popover);
}

void PresetListView::setPalette (const PanelPalette& p)
{
    pal = p;
    repaint();
    content->repaint();
}

void PresetListView::setPopover (juce::Component* p, std::function<int (int)> heightForWidth)
{
    popover = p;
    popoverHeight = std::move (heightForWidth);
    if (popover != nullptr) content->addChildComponent (*popover);
}

int PresetListView::baseRowHeight (const PresetEntry& e) const
{
    const bool comment = model != nullptr && model->showComments() && e.meta.comment.isNotEmpty();
    return rowPadY * 2 + lineH + (comment ? commentH : 0);
}

juce::Rectangle<int> PresetListView::rowBounds (int row) const
{
    if (! juce::isPositiveAndBelow (row, (int) rowTops.size())) return {};
    return { 0, rowTops[(size_t) row], content->getWidth(), rowHeights[(size_t) row] };
}

int PresetListView::rowAt (int y) const
{
    // Rows are few enough per screen that a binary search is the only cleverness needed.
    auto it = std::upper_bound (rowTops.begin(), rowTops.end(), y);
    if (it == rowTops.begin()) return -1;
    const int row = (int) std::distance (rowTops.begin(), it) - 1;
    return y < rowTops[(size_t) row] + rowHeights[(size_t) row] ? row : -1;
}

PresetListView::RowGeometry PresetListView::geometryFor (const PresetEntry& e, juce::Rectangle<int> row) const
{
    RowGeometry g;
    auto line = juce::Rectangle<int> (row.getX() + rowPadX, row.getY() + rowPadY,
                                      row.getWidth() - rowPadX * 2, lineH);

    g.star = line.removeFromLeft (14).withSizeKeepingCentre (14, 14);
    line.removeFromLeft (indent - 14 - 13 + 9);   // 9 px gap, as the design's flex gap

    // Right-aligned meta first: it is fixed text, the name ellipsises into what is left.
    juce::StringArray metaParts;
    if (e.category.isNotEmpty()) metaParts.add (e.category);
    if (e.meta.author.isNotEmpty()) metaParts.add (e.meta.author);
    if (e.meta.date.isNotEmpty()) metaParts.add (e.meta.date.substring (0, 10));
    const auto metaText = metaParts.joinIntoString ("  " + middot + "  ");
    const int metaW = metaText.isEmpty() ? 0 : textWidth (PanelFonts::mono (11.0f), metaText);
    g.meta = line.removeFromRight (juce::jmin (metaW, line.getWidth() / 2));
    line.removeFromRight (12);

    const auto nameFont = PanelFonts::sans (13.0f, true);
    const int nameW = juce::jmin (textWidth (nameFont, displayName (e)) + 2,
                                  juce::jmax (60, line.getWidth() / 2));
    g.name = line.removeFromLeft (nameW);
    line.removeFromLeft (9);

    const auto badgeFont = PanelFonts::mono (9.5f);
    if (e.isBuiltIn)
    {
        g.factoryBadge = line.removeFromLeft (textWidth (badgeFont, "FACTORY") + 12).withSizeKeepingCentre (
            textWidth (badgeFont, "FACTORY") + 12, 16);
        line.removeFromLeft (9);
    }
    if (model != nullptr && model->isRowOpen (e))
    {
        g.openBadge = line.removeFromLeft (textWidth (badgeFont, "OPEN") + 12).withSizeKeepingCentre (
            textWidth (badgeFont, "OPEN") + 12, 16);
        line.removeFromLeft (9);
    }

    const auto tagFont = PanelFonts::sans (10.5f);
    for (const auto& t : e.meta.tags)
    {
        const int w = textWidth (tagFont, t) + 14;
        if (w > line.getWidth()) break;   // clipped rather than squeezed: the rail lists them all
        g.tags.push_back ({ line.removeFromLeft (w).withSizeKeepingCentre (w, 18), t });
        line.removeFromLeft (4);
    }

    if (model != nullptr && model->showComments() && e.meta.comment.isNotEmpty())
        g.comment = { row.getX() + rowPadX + indent, row.getY() + rowPadY + lineH + 3,
                      row.getWidth() - rowPadX * 2 - indent, commentH - 3 };
    return g;
}

void PresetListView::paintRows (juce::Graphics& g, juce::Rectangle<int> clip)
{
    const auto& c = pal.base;
    g.fillAll (c.background);

    if (numRows == 0)
    {
        g.setFont (PanelFonts::sans (12.5f));
        g.setColour (pal.textFaint);
        g.drawText ("no preset matches the filter", 0, emptyTop, content->getWidth(),
                    90, juce::Justification::centred, false);
        return;
    }
    if (model == nullptr) return;

    const int first = juce::jmax (0, rowAt (clip.getY()) >= 0 ? rowAt (clip.getY()) : 0);
    for (int row = first; row < numRows; ++row)
    {
        const int top = rowTops[(size_t) row];
        if (top > clip.getBottom()) break;
        const auto* e = model->getRow (row);
        if (e == nullptr) continue;

        const auto bounds = rowBounds (row);
        const int blockBottom = row + 1 < numRows ? rowTops[(size_t) row + 1] : bounds.getBottom() + 1;
        if (blockBottom < clip.getY()) continue;

        juce::Graphics::ScopedSaveState save (g);
        if (row == draggingRow) g.beginTransparencyLayer (0.45f);

        const bool selected = row == selectedRow;
        if (selected)            g.setColour (pal.rowSelected);
        else if (row == hoverRow) g.setColour (pal.rowHover);
        else                     g.setColour (juce::Colours::transparentBlack);
        g.fillRect (bounds);

        const bool open = model->isRowOpen (*e);
        if (open)
        {
            g.setColour (c.accent);
            g.fillRect (bounds.withWidth (2));
        }

        const auto geo = geometryFor (*e, bounds);

        const bool fav = model->isRowFavourite (*e);
        g.setFont (PanelFonts::sans (13.0f));
        g.setColour (fav ? c.favourite : pal.starOff);
        g.drawText (fav ? juce::String (juce::CharPointer_UTF8 ("\xe2\x98\x85"))
                        : juce::String (juce::CharPointer_UTF8 ("\xe2\x98\x86")),
                    geo.star, juce::Justification::centred, false);

        g.setFont (PanelFonts::sans (13.0f, true));
        g.setColour (c.text);
        g.drawText (displayName (*e), geo.name, juce::Justification::centredLeft, true);

        const auto badgeFont = PanelFonts::mono (9.5f);
        if (! geo.factoryBadge.isEmpty())
        {
            g.setColour (c.outline);
            g.drawRoundedRectangle (geo.factoryBadge.toFloat().reduced (0.5f), 2.0f, 1.0f);
            g.setFont (badgeFont);
            g.setColour (c.textDim);
            g.drawText ("FACTORY", geo.factoryBadge, juce::Justification::centred, false);
        }
        if (! geo.openBadge.isEmpty())
        {
            g.setColour (pal.accentFill);
            g.fillRoundedRectangle (geo.openBadge.toFloat(), 2.0f);
            g.setFont (badgeFont);
            g.setColour (pal.accentLight);
            g.drawText ("OPEN", geo.openBadge, juce::Justification::centred, false);
        }

        g.setFont (PanelFonts::sans (10.5f));
        for (const auto& [area, tag] : geo.tags)
        {
            const bool filtering = model->isTagFiltering (tag);
            // The selected row's fill is close to the outline colour, so it is lifted there.
            g.setColour (filtering ? c.accent : selected ? c.outline.brighter (0.35f) : c.outline);
            g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 9.0f, 1.0f);
            g.setColour (filtering ? pal.accentText : c.textDim);
            g.drawText (tag, area, juce::Justification::centred, false);
        }

        juce::StringArray metaParts;
        if (e->category.isNotEmpty()) metaParts.add (e->category);
        if (e->meta.author.isNotEmpty()) metaParts.add (e->meta.author);
        if (e->meta.date.isNotEmpty()) metaParts.add (e->meta.date.substring (0, 10));
        g.setFont (PanelFonts::mono (11.0f));
        g.setColour (c.textDim);
        g.drawText (metaParts.joinIntoString ("  " + middot + "  "), geo.meta,
                    juce::Justification::centredRight, true);

        if (! geo.comment.isEmpty())
        {
            g.setFont (PanelFonts::sans (11.5f));
            g.setColour (c.textDim.interpolatedWith (c.background, 0.12f));
            g.drawText (e->meta.comment, geo.comment, juce::Justification::centredLeft, true);
        }

        if (selected && hintArea.getWidth() > 20)
        {
            g.setFont (PanelFonts::sans (11.0f));
            g.setColour (pal.textFaint);
            g.drawText (hint, hintArea, juce::Justification::centredLeft, true);
        }

        if (row == draggingRow) g.endTransparencyLayer();

        g.setColour (pal.rowDivider);
        g.fillRect (0, blockBottom - 1, content->getWidth(), 1);
    }
}

void PresetListView::updateContent()
{
    numRows = model != nullptr ? model->getNumRows() : 0;
    if (selectedRow >= numRows) selectedRow = -1;
    if (popoverRow >= numRows) popoverRow = -1;
    hoverRow = -1;
    relayout();
    content->repaint();
}

void PresetListView::relayout()
{
    const int w = viewport.getMaximumVisibleWidth();
    rowTops.assign ((size_t) numRows, 0);
    rowHeights.assign ((size_t) numRows, 0);

    int y = 0;
    const bool showPopover = popover != nullptr && popoverHeight != nullptr;

    if (popoverLive && showPopover)
    {
        const int pw = w - 24;
        const int ph = popoverHeight (pw);
        popover->setBounds (12, y + 10, pw, ph);
        y += 10 + ph + 14;
    }

    emptyTop = y;

    for (int row = 0; row < numRows; ++row)
    {
        const auto* e = model->getRow (row);
        rowTops[(size_t) row] = y;
        int h = e != nullptr ? baseRowHeight (*e) : lineH;
        if (row == selectedRow) h += stripH;
        rowHeights[(size_t) row] = h;
        y += h;

        if (row == popoverRow && showPopover)
        {
            const int px = rowPadX + indent;
            const int pw = w - px - 12;
            const int ph = popoverHeight (pw);
            popover->setBounds (px, y, pw, ph);
            y += ph + 12;
        }
        y += 1;   // divider
    }

    if (numRows == 0) y += 90;

    if (popover != nullptr)
        popover->setVisible (showPopover && (popoverLive || juce::isPositiveAndBelow (popoverRow, numRows)));

    content->setSize (w, juce::jmax (y, viewport.getHeight()));
    layoutActionStrip();
    content->repaint();
}

void PresetListView::layoutActionStrip()
{
    const bool show = juce::isPositiveAndBelow (selectedRow, numRows) && model != nullptr;
    for (auto* b : { &loadButton, &auditionButton, &editButton, &duplicateButton, &deleteButton })
        b->setVisible (false);
    hintArea = {};
    if (! show) return;

    const auto* e = model->getRow (selectedRow);
    if (e == nullptr) return;

    int x = rowPadX + indent;
    const int y = rowTops[(size_t) selectedRow] + baseRowHeight (*e) - rowPadY + 8;
    const auto f = PanelFonts::sans (12.0f);
    auto place = [&] (juce::TextButton& b, int extra)
    {
        const int w = textWidth (f, b.getButtonText()) + extra;
        b.setBounds (x, y, w, 26);
        b.setVisible (true);
        x += w + 6;
    };

    place (loadButton, 26);
    if (model->hasAuditioner()) place (auditionButton, 22);
    place (editButton, 22);
    place (duplicateButton, 22);
    place (deleteButton, 22);

    updateActionButtons();

    hint = e->isBuiltIn ? juce::String (juce::CharPointer_UTF8 ("factory preset \xe2\x80\x94 edit writes a copy into your folder"))
                        : juce::String ("double-click a row to load it");
    hintArea = { x + 4, y, content->getWidth() - x - 16, 26 };
}

void PresetListView::updateActionButtons()
{
    if (model == nullptr || ! juce::isPositiveAndBelow (selectedRow, numRows)) return;
    const auto* e = model->getRow (selectedRow);
    if (e == nullptr) return;

    deleteButton.setEnabled (! e->isBuiltIn);
    deleteButton.setTooltip (e->isBuiltIn ? "factory presets cannot be deleted -- duplicate one to get "
                                            "an editable copy"
                                          : "move it to the system trash");
    editButton.setTooltip (e->isBuiltIn ? "a factory preset cannot be written to -- apply writes an "
                                          "editable copy into your folder"
                                        : "name, author, comment, description and tags");
    auditionButton.setEnabled (model->canAudition (*e));
}

void PresetListView::selectRow (int row, bool notify)
{
    if (! juce::isPositiveAndBelow (row, numRows)) row = -1;
    const bool changed = row != selectedRow;
    selectedRow = row;

    // The popover belongs to the row it was opened on; selecting another closes it.
    if (changed && popoverRow >= 0 && popoverRow != row) popoverRow = -1;

    relayout();
    if (row >= 0) scrollToRow (row);
    if (notify && model != nullptr) model->rowSelected (row);
}

void PresetListView::showPopoverForRow (int row)
{
    popoverLive = false;
    popoverRow = juce::isPositiveAndBelow (row, numRows) ? row : -1;
    relayout();
    if (popoverRow >= 0 && popover != nullptr)
    {
        // Bring as much of the popover into view as fits, the row above it included.
        const int top = rowTops[(size_t) popoverRow];
        const int bottom = popover->getBottom() + 12;
        const int viewH = viewport.getViewHeight();
        int pos = viewport.getViewPositionY();
        if (bottom > pos + viewH) pos = juce::jmin (top, bottom - viewH);
        if (top < pos) pos = top;
        viewport.setViewPosition (0, pos);
    }
}

void PresetListView::showPopoverForLive()
{
    popoverRow = -1;
    popoverLive = true;
    relayout();
    viewport.setViewPosition (0, 0);
}

void PresetListView::hidePopover()
{
    popoverRow = -1;
    popoverLive = false;
    relayout();
}

void PresetListView::setDraggingRow (int row)
{
    draggingRow = row;
    content->repaint();
}

void PresetListView::scrollToRow (int row)
{
    if (! juce::isPositiveAndBelow (row, (int) rowTops.size())) return;
    const int top = rowTops[(size_t) row];
    const int bottom = top + rowHeights[(size_t) row];
    const int pos = viewport.getViewPositionY();
    const int viewH = viewport.getViewHeight();
    if (top < pos) viewport.setViewPosition (0, top);
    else if (bottom > pos + viewH) viewport.setViewPosition (0, bottom - viewH);
}

void PresetListView::scrollToTop() { viewport.setViewPosition (0, 0); }

void PresetListView::resized()
{
    viewport.setBounds (getLocalBounds());
    relayout();
}

bool PresetListView::keyPressed (const juce::KeyPress& k)
{
    if (model == nullptr) return false;

    if (k == juce::KeyPress::upKey || k == juce::KeyPress::downKey)
    {
        if (numRows == 0) return true;
        const int step = k == juce::KeyPress::upKey ? -1 : 1;
        const int next = selectedRow < 0 ? (step > 0 ? 0 : numRows - 1)
                                         : juce::jlimit (0, numRows - 1, selectedRow + step);
        if (next != selectedRow) selectRow (next);
        return true;
    }
    if (k == juce::KeyPress::homeKey)  { if (numRows > 0) selectRow (0); return true; }
    if (k == juce::KeyPress::endKey)   { if (numRows > 0) selectRow (numRows - 1); return true; }
    if (k == juce::KeyPress::returnKey)
    {
        if (selectedRow >= 0) model->rowLoadRequested (selectedRow);
        return true;
    }
    if (k == juce::KeyPress::F2Key)
    {
        if (selectedRow >= 0) model->rowEditRequested (selectedRow);
        return true;
    }
    if (k == juce::KeyPress::escapeKey)
    {
        model->escapePressed();
        return true;
    }
    return false;
}

void PresetListView::showRowMenu (int row)
{
    if (model == nullptr) return;
    const auto* e = model->getRow (row);
    if (e == nullptr) return;
    const bool writable = ! e->isBuiltIn;

    juce::PopupMenu m;
    m.addItem (1, "load");
    m.addItem (2, "edit" + ellipsis);
    m.addItem (3, "duplicate");
    m.addItem (4, "show in file manager", writable);
    m.addSeparator();
    m.addItem (5, "delete" + ellipsis, writable);

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                     [safe = juce::Component::SafePointer<PresetListView> (this), row] (int result)
    {
        auto* o = safe.getComponent();
        if (o == nullptr || o->model == nullptr || row != o->selectedRow) return;
        switch (result)
        {
            case 1: o->model->rowLoadRequested (row); break;
            case 2: o->model->rowEditRequested (row); break;
            case 3: o->model->rowDuplicateRequested (row); break;
            case 4: o->model->rowRevealRequested (row); break;
            case 5: o->model->rowDeleteRequested (row); break;
            default: break;
        }
    });
}

// ── modal ───────────────────────────────────────────────────────────────────

PresetModal::PresetModal (const PanelPalette& p, int width) : pal (p), cardWidth (width)
{
    setWantsKeyboardFocus (true);
    setAlwaysOnTop (true);
}

void PresetModal::setTitle (const juce::String& t) { title = t; }

void PresetModal::addText (const juce::String& t)
{
    auto part = std::make_unique<Part>();
    part->kind = Kind::text;
    part->text = t;
    parts.push_back (std::move (part));
}

void PresetModal::addCaption (const juce::String& t)
{
    auto part = std::make_unique<Part>();
    part->kind = Kind::caption;
    part->text = t;
    parts.push_back (std::move (part));
}

void PresetModal::addPathBox (const juce::String& t)
{
    auto part = std::make_unique<Part>();
    part->kind = Kind::pathBox;
    part->text = t;
    parts.push_back (std::move (part));
}

juce::Label& PresetModal::addSummary (const juce::String& t)
{
    auto part = std::make_unique<Part>();
    part->kind = Kind::summary;
    auto label = std::make_unique<juce::Label> ("summary", t);
    label->setFont (PanelFonts::sans (12.0f));
    label->setColour (juce::Label::textColourId, pal.base.text.interpolatedWith (pal.base.textDim, 0.6f));
    label->setJustificationType (juce::Justification::topLeft);
    label->setMinimumHorizontalScale (1.0f);
    addAndMakeVisible (*label);
    auto& ref = *label;
    part->component = std::move (label);
    parts.push_back (std::move (part));
    return ref;
}

juce::TextEditor& PresetModal::addPathInput (const juce::String& placeholder, juce::TextButton*& browseOut)
{
    auto part = std::make_unique<Part>();
    part->kind = Kind::pathInput;
    auto editor = std::make_unique<juce::TextEditor> ("destination folder");
    editor->setTextToShowWhenEmpty (placeholder, pal.textFaint);
    styleEditor (*editor, pal, 11.5f, true);
    addAndMakeVisible (*editor);
    part->browse = std::make_unique<juce::TextButton> ("browse" + ellipsis);
    setButtonStyle (*part->browse, ButtonStyle::plain);
    addAndMakeVisible (*part->browse);
    browseOut = part->browse.get();
    auto& ref = *editor;
    part->component = std::move (editor);
    parts.push_back (std::move (part));
    return ref;
}

juce::TextButton& PresetModal::addButton (const juce::String& text, ButtonStyle style, std::function<void()> fn)
{
    auto* b = buttons.add (new juce::TextButton (text));
    setButtonStyle (*b, style);
    b->onClick = std::move (fn);
    addAndMakeVisible (b);
    return *b;
}

juce::TextButton& PresetModal::addCancel (const juce::String& text, std::function<void()> fn)
{
    cancel = std::make_unique<juce::TextButton> (text);
    setButtonStyle (*cancel, ButtonStyle::quiet);
    cancelAction = fn;
    cancel->onClick = std::move (fn);
    addAndMakeVisible (*cancel);
    return *cancel;
}

juce::TextButton* PresetModal::findButton (const juce::String& text) const
{
    for (auto* b : buttons)
        if (b->getButtonText() == text) return b;
    if (cancel != nullptr && cancel->getButtonText() == text) return cancel.get();
    return nullptr;
}

int PresetModal::heightOfPart (Part& p, int width) const
{
    auto paragraphHeight = [] (const juce::String& text, const juce::Font& f, int w)
    {
        juce::AttributedString s;
        s.append (text, f);
        s.setLineSpacing (3.0f);
        juce::TextLayout tl;
        tl.createLayout (s, (float) w);
        return (int) std::ceil (tl.getHeight());
    };

    switch (p.kind)
    {
        case Kind::text:      return paragraphHeight (p.text, PanelFonts::sans (12.5f), width) + 12;
        case Kind::caption:   return 18;
        case Kind::pathBox:   return 28 + 14;
        case Kind::pathInput: return 28 + 14;
        case Kind::summary:
        {
            auto* label = dynamic_cast<juce::Label*> (p.component.get());
            const auto text = label != nullptr ? label->getText() : juce::String();
            return juce::jmax (40, paragraphHeight (text, PanelFonts::sans (12.0f), width - 24) + 22) + 16;
        }
    }
    return 0;
}

void PresetModal::resized()
{
    const int w = juce::jmin (cardWidth, getWidth() - 32);
    const int inner = w - 36;

    int h = 18 + 22 + 8;
    for (auto& p : parts) h += heightOfPart (*p, inner);
    h += 30 + 18;

    card = juce::Rectangle<int> (w, h).withCentre (getLocalBounds().getCentre());

    int y = card.getY() + 18 + 22 + 8;
    const int x = card.getX() + 18;
    for (auto& p : parts)
    {
        const int ph = heightOfPart (*p, inner);
        p->area = { x, y, inner, ph };
        if (p->kind == Kind::summary && p->component != nullptr)
            p->component->setBounds (x + 12, y + 10, inner - 24, ph - 16 - 20);
        if (p->kind == Kind::pathInput && p->component != nullptr)
        {
            const int bw = 82;
            p->component->setBounds (x, y, inner - bw - 8, 28);
            p->browse->setBounds (x + inner - bw, y, bw, 28);
        }
        y += ph;
    }

    int bx = x;
    const auto f = PanelFonts::sans (12.0f, true);
    for (auto* b : buttons)
    {
        const int bw = textWidth (f, b->getButtonText()) + 28;
        b->setBounds (bx, y, bw, 30);
        bx += bw + 8;
    }
    if (cancel != nullptr)
    {
        const int cw = textWidth (f, cancel->getButtonText()) + 24;
        cancel->setBounds (x + inner - cw, y, cw, 30);
    }
}

void PresetModal::paint (juce::Graphics& g)
{
    g.fillAll (pal.scrim);

    juce::DropShadow (juce::Colours::black.withAlpha (0.55f), 40, { 0, 16 }).drawForRectangle (g, card);
    g.setColour (pal.card);
    g.fillRoundedRectangle (card.toFloat(), 4.0f);
    g.setColour (pal.cardOutline);
    g.drawRoundedRectangle (card.toFloat().reduced (0.5f), 4.0f, 1.0f);

    g.setFont (PanelFonts::sans (14.0f, true));
    g.setColour (pal.base.text);
    g.drawText (title, card.getX() + 18, card.getY() + 18, card.getWidth() - 36, 22,
                juce::Justification::centredLeft, true);

    for (auto& p : parts)
    {
        switch (p->kind)
        {
            case Kind::text:
            {
                juce::AttributedString s;
                s.append (p->text, PanelFonts::sans (12.5f),
                          pal.base.text.interpolatedWith (pal.base.textDim, 0.6f));
                s.setLineSpacing (3.0f);
                juce::TextLayout tl;
                tl.createLayout (s, (float) p->area.getWidth());
                tl.draw (g, p->area.toFloat());
                break;
            }
            case Kind::caption:
                g.setFont (PanelFonts::sans (11.0f));
                g.setColour (pal.base.textDim);
                g.drawText (p->text, p->area.withTrimmedBottom (5), juce::Justification::bottomLeft, true);
                break;
            case Kind::pathBox:
            {
                const auto box = p->area.withHeight (28);
                g.setColour (pal.field.brighter (0.05f));
                g.fillRoundedRectangle (box.toFloat(), 3.0f);
                g.setColour (pal.divider.brighter (0.1f));
                g.drawRoundedRectangle (box.toFloat().reduced (0.5f), 3.0f, 1.0f);
                g.setFont (PanelFonts::mono (11.5f));
                g.setColour (pal.base.textDim);
                g.drawText (p->text, box.reduced (8, 0), juce::Justification::centredLeft, true);
                break;
            }
            case Kind::summary:
            {
                const auto box = p->area.withTrimmedBottom (16);
                g.setColour (pal.field.brighter (0.05f));
                g.fillRoundedRectangle (box.toFloat(), 3.0f);
                g.setColour (pal.divider.brighter (0.1f));
                g.drawRoundedRectangle (box.toFloat().reduced (0.5f), 3.0f, 1.0f);
                break;
            }
            case Kind::pathInput:
                break;
        }
    }
}

bool PresetModal::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey && cancelAction)
    {
        auto fn = cancelAction;   // may delete this
        fn();
        return true;
    }
    if (k == juce::KeyPress::returnKey)
    {
        // Return is the emphasised button -- which is never the destructive one.
        for (auto* b : buttons)
            if (getButtonStyle (*b) == ButtonStyle::accent && b->isEnabled() && b->onClick)
            {
                auto fn = b->onClick;
                fn();
                return true;
            }
    }
    return true;   // modal: nothing leaks to the panel underneath
}

} // namespace atl
