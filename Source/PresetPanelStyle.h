#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace atl
{

/** Colours the panel draws with. Defaults are a dark neutral set, so the panel reads as chrome
    inside any plugin skin; a plugin with its own theme passes its own so the tab does not look
    like a guest. Everything else the panel paints -- bars, fields, selected rows, the live strip --
    is derived from these (see `PanelPalette`), so a plugin never has to name more than this. */
struct PresetPanelColours
{
    juce::Colour background { 0xff1a1a1c };
    juce::Colour panel      { 0xff232327 };
    juce::Colour text       { 0xffe4e4e8 };
    juce::Colour textDim    { 0xff9b9ba3 };
    /** Used for exactly two things: the live patch, and the current filter. */
    juce::Colour accent     { 0xff4aa3ff };
    juce::Colour outline    { 0xff3a3a40 };

    /** The "unsaved changes" chip. */
    juce::Colour warning    { 0xffe8d49a };
    /** Text of a button that destroys something. */
    juce::Colour danger     { 0xffe8b4b4 };
    /** A favourite's star. */
    juce::Colour favourite  { 0xffe0c98a };
};

/** The shades the panel actually paints with, derived once from `PresetPanelColours`. Kept in one
    place so a plugin's palette lands consistently everywhere rather than being mixed ad hoc. */
struct PanelPalette
{
    PresetPanelColours base;

    juce::Colour bar;            // folder bar, status bar
    juce::Colour rail;           // tag rail, active filter bar
    juce::Colour divider;        // 1 px separators between bars
    juce::Colour rowDivider;
    juce::Colour field;          // text editor background
    juce::Colour button;         // a plain button's fill
    juce::Colour buttonHover;
    juce::Colour card;           // popover and dialog background
    juce::Colour cardOutline;
    juce::Colour rowHover;
    juce::Colour rowSelected;
    juce::Colour liveStrip;
    juce::Colour accentFill;     // tinted background of an accent chip or badge
    juce::Colour accentText;     // text on `accentFill`
    juce::Colour accentLight;    // accent used as text on dark
    juce::Colour onAccent;       // text on a solid accent button
    juce::Colour textFaint;      // hints, counts
    juce::Colour disabledText;
    juce::Colour disabledFill;
    juce::Colour warningFill, warningOutline;
    juce::Colour dangerFill, dangerOutline, dangerStrong;
    juce::Colour scrim;          // behind a dialog

    static PanelPalette from (const PresetPanelColours&);
};

/** How a button draws. Set once per button with `setButtonStyle`; the look-and-feel reads it
    back, so a button's role is declared where it is created rather than re-derived when painted. */
enum class ButtonStyle
{
    plain,         // outlined, the default
    accent,        // solid accent: the action that loses nothing
    quiet,         // text only: cancel
    danger,        // outlined, reddish text: delete
    destructive,   // red fill: the one-way version, only ever inside a confirmation
    chip,          // rounded pill: a tag
    chipActive,    // rounded pill in accent: a tag that is filtering
    chipDashed,    // rounded, dashed: a suggestion
    segment,       // one half of a segmented toggle; toggle state = on
    bare           // no background at all (the star, the close cross)
};

void setButtonStyle (juce::Button&, ButtonStyle);
ButtonStyle getButtonStyle (const juce::Button&);

/** Neutral grotesque for everything a person wrote, mono for what a machine did: paths, dates,
    counts, the extension. Sizes are the design's CSS pixels. */
namespace PanelFonts
{
    juce::Font sans (float cssPx, bool medium = false);
    juce::Font mono (float cssPx);
    /** Upper-case, letter-spaced label: `PRESETS`, `TAGS`, `LIVE PATCH`. */
    juce::Font caps (float cssPx);
    float width (const juce::Font&, const juce::String&);
}

/** Flat, 3 px radii, no gradients: chrome, not decoration. */
class PresetPanelLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PresetPanelLookAndFeel();

    void setPalette (const PanelPalette&);
    const PanelPalette& palette() const { return pal; }

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool isHighlighted, bool isDown) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool isHighlighted, bool isDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isDown, int buttonX, int buttonY,
                       int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;

    void fillTextEditorBackground (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int width, int height, juce::TextEditor&) override;

    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height,
                        bool isVertical, int thumbStart, int thumbSize, bool isMouseOver,
                        bool isMouseDown) override;
    int getDefaultScrollbarWidth() override { return 10; }

    void drawTooltip (juce::Graphics&, const juce::String&, int width, int height) override;

private:
    PanelPalette pal;
};

/** A text editor styled as one of the panel's fields. `pill` is the tag input's rounded shape. */
void styleEditor (juce::TextEditor&, const PanelPalette&, float cssPx = 12.5f, bool mono = false,
                  bool pill = false);

} // namespace atl
