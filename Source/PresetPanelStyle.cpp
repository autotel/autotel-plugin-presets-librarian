#include "PresetPanelStyle.h"

namespace atl
{

namespace
{
const juce::Identifier styleProperty { "atlButtonStyle" };
const juce::Identifier pillProperty  { "atlPill" };

constexpr float radius = 3.0f;
constexpr float pillRadius = 11.0f;

/** CSS pixels to a JUCE font height: a JUCE height covers ascent plus descent, which runs about
    a tenth larger than the em size a stylesheet names. */
float toHeight (float cssPx) { return cssPx * 1.12f; }
} // namespace

// ── palette ─────────────────────────────────────────────────────────────────

PanelPalette PanelPalette::from (const PresetPanelColours& c)
{
    PanelPalette p;
    p.base = c;

    // The derivations were written for a dark panel, and the ones that push toward black or white
    // did so by assumption. On a light palette that put dark text on the accent button, a black
    // slab under the search text and an invisible empty star, so those follow the background's
    // polarity. A dark palette derives exactly what it did.
    const bool light = c.background.getPerceivedBrightness() > 0.5f;
    const auto ink   = juce::Colours::black;

    p.bar          = c.background.interpolatedWith (c.panel, 0.4f);
    p.rail         = c.background.interpolatedWith (c.panel, 0.25f);
    p.divider      = c.panel.interpolatedWith (c.outline, 0.5f);
    p.rowDivider   = c.background.interpolatedWith (c.panel, 0.9f);
    p.field        = light ? c.background.interpolatedWith (juce::Colours::white, 0.6f)
                           : c.background.darker (0.25f);
    p.button       = c.panel.interpolatedWith (c.outline, 0.17f);
    p.buttonHover  = c.panel.interpolatedWith (c.outline, 0.5f);
    p.card         = c.panel.interpolatedWith (c.outline, 0.12f);
    p.cardOutline  = c.outline.interpolatedWith (c.text, 0.06f);
    p.rowHover     = c.background.interpolatedWith (c.panel, 0.6f);
    p.rowSelected  = c.panel.interpolatedWith (c.accent, 0.06f).brighter (0.05f);
    p.liveStrip    = c.background.interpolatedWith (c.accent, 0.07f);
    p.accentFill   = c.background.interpolatedWith (c.accent, 0.14f);
    p.accentText   = light ? c.accent.interpolatedWith (ink, 0.35f)
                           : c.accent.interpolatedWith (juce::Colours::white, 0.75f);
    p.accentLight  = light ? c.accent.interpolatedWith (ink, 0.25f)
                           : c.accent.interpolatedWith (juce::Colours::white, 0.3f);
    p.onAccent     = ! light ? c.background.darker (0.4f)
                   : c.accent.getPerceivedBrightness() > 0.6f ? c.text : juce::Colours::white;
    p.textFaint    = c.textDim.interpolatedWith (c.background, 0.3f);
    p.disabledText = c.textDim.interpolatedWith (c.background, 0.5f);
    p.starOff      = light ? p.textFaint : c.outline.brighter (0.2f);   // brightening an outline is invisible on light
    p.disabledFill = c.background.interpolatedWith (c.panel, 0.4f);
    p.warningFill    = c.background.interpolatedWith (c.warning, 0.1f);
    p.warningOutline = c.background.interpolatedWith (c.warning, 0.38f);
    p.dangerFill     = c.background.interpolatedWith (juce::Colour (0xffa04040), 0.3f);
    p.dangerOutline  = c.background.interpolatedWith (juce::Colour (0xffc05050), 0.5f);
    p.dangerStrong   = light ? c.danger.interpolatedWith (ink, 0.2f)
                             : c.danger.interpolatedWith (juce::Colours::white, 0.4f);
    p.scrim          = light ? ink.withAlpha (0.3f) : c.background.darker (0.5f).withAlpha (0.68f);
    return p;
}

// ── button style ────────────────────────────────────────────────────────────

void setButtonStyle (juce::Button& b, ButtonStyle s)
{
    b.getProperties().set (styleProperty, (int) s);
    b.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    b.repaint();
}

ButtonStyle getButtonStyle (const juce::Button& b)
{
    return (ButtonStyle) (int) b.getProperties().getWithDefault (styleProperty, (int) ButtonStyle::plain);
}

// ── fonts ───────────────────────────────────────────────────────────────────

namespace PanelFonts
{
juce::Font sans (float cssPx, bool medium)
{
    return juce::Font (juce::FontOptions (toHeight (cssPx), medium ? juce::Font::bold : juce::Font::plain));
}

juce::Font mono (float cssPx)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), toHeight (cssPx),
                                          juce::Font::plain));
}

juce::Font caps (float cssPx)
{
    return sans (cssPx).withExtraKerningFactor (0.09f);
}

float width (const juce::Font& f, const juce::String& s)
{
    return juce::GlyphArrangement::getStringWidth (f, s);
}
} // namespace PanelFonts

// ── look and feel ───────────────────────────────────────────────────────────

PresetPanelLookAndFeel::PresetPanelLookAndFeel()
{
    setPalette (PanelPalette::from ({}));
}

void PresetPanelLookAndFeel::setPalette (const PanelPalette& p)
{
    pal = p;
    const auto& c = p.base;

    setColour (juce::TextButton::buttonColourId, p.button);
    setColour (juce::TextButton::textColourOffId, c.text);
    setColour (juce::TextButton::textColourOnId, c.text);

    setColour (juce::ComboBox::backgroundColourId, p.button);
    setColour (juce::ComboBox::textColourId, c.text);
    setColour (juce::ComboBox::outlineColourId, c.outline);
    setColour (juce::ComboBox::arrowColourId, c.textDim);

    setColour (juce::PopupMenu::backgroundColourId, p.card);
    setColour (juce::PopupMenu::textColourId, c.text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, p.accentFill);
    setColour (juce::PopupMenu::highlightedTextColourId, p.accentText);

    setColour (juce::TextEditor::backgroundColourId, p.field);
    setColour (juce::TextEditor::textColourId, c.text);
    setColour (juce::TextEditor::outlineColourId, c.outline);
    setColour (juce::TextEditor::focusedOutlineColourId, c.accent);
    setColour (juce::TextEditor::highlightColourId, c.accent.withAlpha (0.45f));
    setColour (juce::TextEditor::highlightedTextColourId, c.text);
    setColour (juce::CaretComponent::caretColourId, c.text);

    setColour (juce::Label::textColourId, c.text);
    setColour (juce::ScrollBar::thumbColourId, c.outline);
    setColour (juce::TooltipWindow::backgroundColourId, p.card);
    setColour (juce::TooltipWindow::textColourId, c.text);
    setColour (juce::TooltipWindow::outlineColourId, p.cardOutline);
}

void PresetPanelLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                                   const juce::Colour&, bool hover, bool down)
{
    const auto& c = pal.base;
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.isEnabled();

    auto box = [&] (juce::Colour fill, juce::Colour line, float rad)
    {
        g.setColour (fill);
        g.fillRoundedRectangle (r, rad);
        if (! line.isTransparent())
        {
            g.setColour (line);
            g.drawRoundedRectangle (r, rad, 1.0f);
        }
    };

    switch (getButtonStyle (b))
    {
        case ButtonStyle::plain:
        case ButtonStyle::danger:
            if (! on) box (pal.disabledFill, c.outline.interpolatedWith (c.background, 0.4f), radius);
            else      box (hover || down ? pal.buttonHover : pal.button, c.outline, radius);
            break;

        case ButtonStyle::accent:
            if (! on) box (pal.disabledFill, c.outline.interpolatedWith (c.background, 0.4f), radius);
            else      box (hover || down ? c.accent.brighter (0.15f) : c.accent, {}, radius);
            break;

        case ButtonStyle::destructive:
            box (hover || down ? pal.dangerFill.brighter (0.15f) : pal.dangerFill, pal.dangerOutline, radius);
            break;

        case ButtonStyle::chip:
            box (hover ? pal.buttonHover : pal.button.brighter (0.05f),
                 hover ? c.outline.brighter (0.3f) : c.outline.brighter (0.1f), pillRadius);
            break;

        case ButtonStyle::chipActive:
            box (pal.accentFill, c.accent, pillRadius);
            break;

        case ButtonStyle::chipDashed:
        {
            juce::Path p;
            p.addRoundedRectangle (r, pillRadius);
            const float dashes[] = { 3.0f, 2.0f };
            juce::Path dashed;
            juce::PathStrokeType (1.0f).createDashedStroke (dashed, p, dashes, 2);
            g.setColour (hover ? c.outline.brighter (0.4f) : c.outline.brighter (0.1f));
            g.fillPath (dashed);
            break;
        }

        case ButtonStyle::segment:
            if (b.getToggleState()) { g.setColour (pal.buttonHover.brighter (0.05f)); g.fillRect (r); }
            break;

        case ButtonStyle::quiet:
        case ButtonStyle::bare:
            break;
    }
}

juce::Font PresetPanelLookAndFeel::getTextButtonFont (juce::TextButton& b, int)
{
    switch (getButtonStyle (b))
    {
        case ButtonStyle::accent:
        case ButtonStyle::destructive: return PanelFonts::sans (12.0f, true);
        case ButtonStyle::chip:
        case ButtonStyle::chipActive:
        case ButtonStyle::chipDashed:  return PanelFonts::sans (11.5f);
        case ButtonStyle::bare:        return PanelFonts::sans (14.0f);
        default:                       return PanelFonts::sans (12.0f);
    }
}

void PresetPanelLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool hover, bool)
{
    const auto& c = pal.base;
    const bool on = b.isEnabled();
    const auto style = getButtonStyle (b);

    juce::Colour col = c.text;
    switch (style)
    {
        case ButtonStyle::plain:       col = on ? c.text : pal.disabledText; break;
        case ButtonStyle::accent:      col = on ? pal.onAccent : pal.disabledText; break;
        case ButtonStyle::quiet:       col = on ? (hover ? c.text : c.textDim) : pal.disabledText; break;
        case ButtonStyle::danger:      col = on ? c.danger : pal.disabledText; break;
        case ButtonStyle::destructive: col = pal.dangerStrong; break;
        case ButtonStyle::chip:        col = c.text; break;
        case ButtonStyle::chipActive:  col = pal.accentText; break;
        case ButtonStyle::chipDashed:  col = hover ? c.text : c.textDim; break;
        case ButtonStyle::segment:     col = b.getToggleState() ? c.text : pal.textFaint; break;
        case ButtonStyle::bare:        col = b.findColour (juce::TextButton::textColourOffId); break;
    }

    // An explicitly set colour wins for a bare button -- the star is gold or grey by state.
    if (style == ButtonStyle::bare && ! hover) col = col.withMultipliedAlpha (0.9f);

    g.setColour (col);
    g.setFont (getTextButtonFont (b, b.getHeight()));
    const int pad = style == ButtonStyle::chip || style == ButtonStyle::chipActive
                        || style == ButtonStyle::chipDashed ? 7 : 4;
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (pad, 0),
                      juce::Justification::centred, 1, 1.0f);
}

void PresetPanelLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int,
                                           juce::ComboBox& box)
{
    // The combo sits inside a joined control with the direction toggle, which draws the outline.
    juce::ignoreUnused (box);
    const auto& c = pal.base;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    juce::Path arrow;
    arrow.addTriangle (ax - 3.5f, ay - 2.0f, ax + 3.5f, ay - 2.0f, ax, ay + 2.5f);
    g.setColour (c.textDim);
    g.fillPath (arrow);
}

juce::Font PresetPanelLookAndFeel::getComboBoxFont (juce::ComboBox&) { return PanelFonts::sans (12.0f); }

void PresetPanelLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Font PresetPanelLookAndFeel::getPopupMenuFont() { return PanelFonts::sans (12.5f); }

void PresetPanelLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    const bool pill = e.getProperties().getWithDefault (pillProperty, false);
    g.setColour (e.findColour (juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle (juce::Rectangle<float> (0, 0, (float) w, (float) h), pill ? pillRadius : radius);
}

void PresetPanelLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    if (! e.isEnabled()) return;
    const bool pill = e.getProperties().getWithDefault (pillProperty, false);
    const bool focused = e.hasKeyboardFocus (true) && ! e.isReadOnly();
    g.setColour (e.findColour (focused ? juce::TextEditor::focusedOutlineColourId
                                       : juce::TextEditor::outlineColourId));
    g.drawRoundedRectangle (juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f),
                            pill ? pillRadius : radius, 1.0f);
}

void PresetPanelLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w,
                                            int h, bool vertical, int thumbStart, int thumbSize,
                                            bool over, bool down)
{
    if (thumbSize <= 0) return;
    auto thumb = vertical ? juce::Rectangle<int> (x, thumbStart, w, thumbSize)
                          : juce::Rectangle<int> (thumbStart, y, thumbSize, h);
    g.setColour (over || down ? pal.base.outline.brighter (0.2f) : pal.base.outline);
    g.fillRoundedRectangle (thumb.toFloat().reduced (3.0f), 3.0f);
}

void PresetPanelLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    g.fillAll (pal.card);
    g.setColour (pal.cardOutline);
    g.drawRect (0, 0, w, h);
    juce::AttributedString s;
    s.setJustification (juce::Justification::centredLeft);
    s.append (text, PanelFonts::sans (11.5f), pal.base.text);
    juce::TextLayout tl;
    tl.createLayoutWithBalancedLineLengths (s, (float) w - 12.0f);
    tl.draw (g, { 6.0f, 0.0f, (float) w - 12.0f, (float) h });
}

void styleEditor (juce::TextEditor& e, const PanelPalette& p, float cssPx, bool mono, bool pill)
{
    e.getProperties().set (pillProperty, pill);
    e.setColour (juce::TextEditor::backgroundColourId, p.field);
    e.setColour (juce::TextEditor::textColourId, p.base.text);
    e.setColour (juce::TextEditor::outlineColourId, p.base.outline);
    e.setColour (juce::TextEditor::focusedOutlineColourId, p.base.accent);
    e.setColour (juce::TextEditor::highlightColourId, p.base.accent.withAlpha (0.45f));
    e.setColour (juce::CaretComponent::caretColourId, p.base.text);
    e.setFont (mono ? PanelFonts::mono (cssPx) : PanelFonts::sans (cssPx));
    e.setIndents (pill ? 9 : 8, 0);
    e.setJustification (e.isMultiLine() ? juce::Justification::topLeft : juce::Justification::centredLeft);
    e.setTextToShowWhenEmpty (e.getTextToShowWhenEmpty(), p.textFaint);
    e.applyFontToAllText (e.getFont());
}

} // namespace atl
