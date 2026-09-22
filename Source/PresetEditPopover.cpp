#include "PresetEditPopover.h"

namespace atl
{

namespace
{
const juce::String times (juce::CharPointer_UTF8 ("\xc3\x97"));
const juce::String ellipsis (juce::CharPointer_UTF8 ("\xe2\x80\xa6"));

constexpr int chipHeight = 22;
constexpr int chipGap = 6;
constexpr int maxSuggestions = 8;

int chipWidth (const juce::String& text)
{
    return (int) std::ceil (PanelFonts::width (PanelFonts::sans (11.5f), text)) + 16;
}
} // namespace

// ── TagPicker ───────────────────────────────────────────────────────────────

TagPicker::TagPicker()
{
    input.setName ("preset tag input");
    input.setTextToShowWhenEmpty ("type to add or create" + ellipsis, pal.textFaint);
    input.onReturnKey = [this]
    {
        addTag (input.getText());
        input.clear();
        rebuild();
    };
    // Suggestions narrow as you type.
    input.onTextChange = [this] { rebuild(); };
    addAndMakeVisible (input);
}

void TagPicker::setPalette (const PanelPalette& p)
{
    pal = p;
    styleEditor (input, pal, 11.5f, false, true);
    input.setTextToShowWhenEmpty ("type to add or create" + ellipsis, pal.textFaint);
}

void TagPicker::setTags (const juce::StringArray& t, const juce::StringArray& vocab)
{
    tags = t;
    vocabulary = vocab;
    input.clear();
    rebuild();
}

void TagPicker::addTag (const juce::String& typed)
{
    auto tag = typed.removeCharacters (",").trim();
    if (tag.isEmpty()) return;

    for (const auto& existing : vocabulary)
        if (existing.equalsIgnoreCase (tag)) { tag = existing; break; }

    for (const auto& t : tags)
        if (t.equalsIgnoreCase (tag)) return;

    tags.add (tag);
    rebuild();
}

void TagPicker::removeTag (const juce::String& tag)
{
    tags.removeString (tag);
    rebuild();
}

void TagPicker::rebuild()
{
    chips.clear();
    suggestions.clear();

    for (const auto& t : tags)
    {
        auto* b = chips.add (new juce::TextButton (t + "  " + times));
        setButtonStyle (*b, ButtonStyle::chip);
        b->setTooltip ("remove this tag");
        b->onClick = [this, t]
        {
            // Deferred: this button is one of the chips about to be rebuilt.
            juce::Component::SafePointer<TagPicker> safe (this);
            juce::MessageManager::callAsync ([safe, t] { if (safe != nullptr) safe->removeTag (t); });
        };
        addAndMakeVisible (b);
    }

    const auto needle = input.getText().trim().toLowerCase();
    for (const auto& t : vocabulary)
    {
        if (suggestions.size() >= maxSuggestions) break;
        if (tags.contains (t, true) || ! t.toLowerCase().contains (needle)) continue;

        auto* b = suggestions.add (new juce::TextButton (t));
        setButtonStyle (*b, ButtonStyle::chipDashed);
        b->setTooltip ("add this tag");
        b->onClick = [this, t]
        {
            juce::Component::SafePointer<TagPicker> safe (this);
            juce::MessageManager::callAsync ([safe, t]
            {
                if (safe == nullptr) return;
                safe->addTag (t);
                safe->input.clear();
                safe->rebuild();
            });
        };
        addAndMakeVisible (b);
    }

    resized();
    if (onLayoutChanged) onLayoutChanged();
}

int TagPicker::layout (int width, bool apply)
{
    int x = 0, y = 0;
    auto place = [&] (juce::Component* c, int w)
    {
        if (x > 0 && x + w > width) { x = 0; y += chipHeight + chipGap; }
        if (apply && c != nullptr) c->setBounds (x, y, juce::jmin (w, width), chipHeight);
        x += w + chipGap;
    };

    for (auto* c : chips) place (c, chipWidth (c->getButtonText()));

    // The input takes the rest of the line, but never less than it needs to be typed into.
    const int minInput = 150;
    if (x > 0 && width - x < minInput) { x = 0; y += chipHeight + chipGap; }
    if (apply) input.setBounds (x, y, width - x, chipHeight + 2);
    y += chipHeight + 2;

    if (! suggestions.isEmpty())
    {
        y += 7;
        x = 0;
        for (auto* s : suggestions) place (s, chipWidth (s->getButtonText()));
        y += chipHeight;
    }
    return y;
}

int TagPicker::getHeightForWidth (int width) { return layout (width, false); }
void TagPicker::resized() { layout (getWidth(), true); }

// ── PresetEditPopover ───────────────────────────────────────────────────────

PresetEditPopover::PresetEditPopover (const juce::String& ext) : extension (ext)
{
    nameEditor.setName ("preset name");
    authorEditor.setName ("preset author");
    commentEditor.setName ("preset comment");
    descriptionEditor.setName ("preset description");
    descriptionEditor.setMultiLine (true, true);
    descriptionEditor.setReturnKeyStartsNewLine (true);

    nameEditor.setTooltip ("the file's name -- a changed name renames the file");
    commentEditor.setTooltip ("a note to yourself that a filename cannot carry -- \"this is the "
                              "newer one\". Shown in the list, never part of the name.");

    for (auto* e : { &nameEditor, &authorEditor, &commentEditor })
    {
        e->onReturnKey = [this] { if (onApply) onApply(); };
        e->onEscapeKey = [this] { if (onCancel) onCancel(); };
    }
    descriptionEditor.onEscapeKey = [this] { if (onCancel) onCancel(); };
    tagPicker.input.onEscapeKey = [this] { if (onCancel) onCancel(); };

    for (auto* e : { &nameEditor, &authorEditor, &commentEditor, &descriptionEditor })
        addAndMakeVisible (*e);
    addAndMakeVisible (tagPicker);
    tagPicker.onLayoutChanged = [this]
    {
        if (onLayoutChanged) onLayoutChanged();
        else resized();
    };

    setButtonStyle (applyButton, ButtonStyle::accent);
    setButtonStyle (cancelButton, ButtonStyle::plain);
    setButtonStyle (closeButton, ButtonStyle::bare);
    closeButton.setTooltip ("close");
    applyButton.onClick = [this] { if (onApply) onApply(); };
    cancelButton.onClick = [this] { if (onCancel) onCancel(); };
    closeButton.onClick = [this] { if (onCancel) onCancel(); };
    for (auto* b : { &applyButton, &cancelButton, &closeButton })
        addAndMakeVisible (*b);

    setWantsKeyboardFocus (false);
}

void PresetEditPopover::setPalette (const PanelPalette& p)
{
    pal = p;
    for (auto* e : { &nameEditor, &authorEditor, &commentEditor, &descriptionEditor })
        styleEditor (*e, pal);
    closeButton.setColour (juce::TextButton::textColourOffId, pal.base.textDim);
    tagPicker.setPalette (pal);
    repaint();
}

void PresetEditPopover::open (const juce::String& h, const juce::String& t, const PresetMetadata& meta,
                              const juce::StringArray& vocabulary, Mode m, bool isLive)
{
    heading = h;
    title = t;
    mode = m;
    live = isLive;
    original = meta;

    nameEditor.setText (meta.name, false);
    authorEditor.setText (meta.author, false);
    commentEditor.setText (meta.comment, false);
    descriptionEditor.setText (meta.description, false);
    tagPicker.setTags (meta.tags, vocabulary);

    applyButton.setButtonText (mode == Mode::saveAs ? "save as" : "apply");
    resized();
    repaint();
}

PresetMetadata PresetEditPopover::getDraft() const
{
    auto m = original;   // keeps `extra` and `date`, which these fields do not show
    m.name = nameEditor.getText().trim();
    m.author = authorEditor.getText().trim();
    m.comment = commentEditor.getText().trim();
    m.description = descriptionEditor.getText();
    m.tags = tagPicker.getTags();
    return m;
}

void PresetEditPopover::setHeader (juce::Component* c, int height)
{
    if (header != nullptr && header != c && header->getParentComponent() == this)
        removeChildComponent (header);

    header = c;
    headerHeight = c != nullptr ? height : 0;
    if (header != nullptr) addAndMakeVisible (*header);
    resized();
}

void PresetEditPopover::focusName()
{
    if (isShowing())
    {
        nameEditor.grabKeyboardFocus();
        nameEditor.selectAll();
    }
}

int PresetEditPopover::getHeightForWidth (int width) { return layout (width, false); }
void PresetEditPopover::resized() { layout (getWidth(), true); }

int PresetEditPopover::layout (int width, bool apply)
{
    constexpr int pad = 14, colGap = 14, rowGap = 11, captionH = 17, fieldH = 28;
    auto set = [apply] (juce::Component& c, juce::Rectangle<int> r) { if (apply) c.setBounds (r); };

    if (apply) captions.clear();
    auto caption = [&] (juce::Rectangle<int> r, const juce::String& text)
    {
        if (apply) captions.push_back ({ r, text });
    };

    const int inner = juce::jmax (120, width - pad * 2);
    int y = pad;

    // heading
    if (apply)
    {
        headingArea = { pad, y, inner - 30, 22 };
        closeButton.setBounds (pad + inner - 24, y - 2, 24, 24);
    }
    y += 22 + 12;

    if (header != nullptr && headerHeight > 0)
    {
        set (*header, { pad, y, inner, headerHeight });
        y += headerHeight + rowGap;
    }

    const int colW = (inner - colGap) / 2;

    // file name · author
    caption ({ pad, y, colW, captionH }, "file name");
    caption ({ pad + colW + colGap, y, colW, captionH }, "author");
    y += captionH;
    {
        const int extW = (int) std::ceil (PanelFonts::width (PanelFonts::mono (11.0f), extension)) + 6;
        set (nameEditor, { pad, y, colW - extW - 6, fieldH });
        if (apply) extensionArea = { pad + colW - extW, y, extW, fieldH };
        set (authorEditor, { pad + colW + colGap, y, colW, fieldH });
    }
    y += fieldH + rowGap;

    caption ({ pad, y, inner, captionH },
              juce::String (juce::CharPointer_UTF8 ("comment \xc2\xb7 shown in the list, never part of the name")));
    y += captionH;
    set (commentEditor, { pad, y, inner, fieldH });
    y += fieldH + rowGap;

    caption ({ pad, y, inner, captionH }, "description");
    y += captionH;
    set (descriptionEditor, { pad, y, inner, 46 });
    y += 46 + rowGap;

    caption ({ pad, y, inner, captionH + 1 }, "tags");
    y += captionH + 1;
    const int tagsH = tagPicker.getHeightForWidth (inner);
    set (tagPicker, { pad, y, inner, tagsH });
    y += tagsH + 14;

    if (apply) dividerY = y;
    y += 12;

    {
        const int applyW = mode == Mode::saveAs ? 74 : 62;
        set (applyButton, { pad, y, applyW, 28 });
        set (cancelButton, { pad + applyW + 8, y, 62, 28 });
        if (apply) hintArea = { pad + applyW + 8 + 62 + 10, y, inner - (applyW + 80), 28 };
    }
    y += 28 + pad;
    return y;
}

void PresetEditPopover::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);

    g.setColour (pal.card);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (pal.cardOutline);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    {
        auto h = headingArea;
        const auto capsFont = PanelFonts::caps (11.0f);
        const auto text = heading.toUpperCase();
        g.setFont (capsFont);
        g.setColour (live ? pal.accentLight : pal.base.textDim);
        const int w = (int) std::ceil (PanelFonts::width (capsFont, text)) + 2;
        g.drawText (text, h.removeFromLeft (w), juce::Justification::centredLeft, false);
        h.removeFromLeft (8);
        g.setFont (PanelFonts::sans (13.0f, true));
        g.setColour (pal.base.text);
        g.drawText (title, h, juce::Justification::centredLeft, true);
    }

    g.setFont (PanelFonts::sans (11.0f));
    g.setColour (pal.base.textDim);
    for (const auto& c : captions)
        g.drawText (c.text, c.area.withTrimmedBottom (4), juce::Justification::bottomLeft, true);

    g.setFont (PanelFonts::mono (11.0f));
    g.setColour (pal.textFaint);
    g.drawText (extension, extensionArea, juce::Justification::centredLeft, false);

    g.setColour (pal.base.outline.withAlpha (0.7f));
    g.fillRect (14, dividerY, getWidth() - 28, 1);

    juce::String hint;
    switch (mode)
    {
        case Mode::edit:        hint = "a name change renames the file on disk"; break;
        case Mode::factoryCopy: hint = "writes an editable copy into your folder"; break;
        case Mode::liveUnsaved: hint = "names the live patch -- save writes it into your folder"; break;
        case Mode::saveAs:      hint = "writes a new file into your folder"; break;
    }
    g.setFont (PanelFonts::sans (11.0f));
    g.setColour (pal.textFaint);
    g.drawText (hint, hintArea, juce::Justification::centredLeft, true);
}

bool PresetEditPopover::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey)
    {
        if (onCancel) onCancel();
        return true;
    }
    return false;
}

} // namespace atl
