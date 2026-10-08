#pragma once

#include <JuceHeader.h>
#include "AerosoundTheme.h"

namespace aerosound::ui
{
class AeroLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    AeroLookAndFeel()
    {
        setColour(juce::Slider::textBoxTextColourId, juce::Colour(ink));
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        // High-contrast selected digits while editing slider value labels.
        setColour(juce::Slider::textBoxHighlightColourId, juce::Colour(0xff166fb0));
        setColour(juce::TextEditor::highlightColourId, juce::Colour(0xff166fb0));
        setColour(juce::TextEditor::highlightedTextColourId, juce::Colours::white);
        setColour(juce::TextEditor::textColourId, juce::Colour(ink));
        setColour(juce::TextEditor::backgroundColourId, juce::Colours::white);
        setColour(juce::Label::textWhenEditingColourId, juce::Colour(ink));
        setColour(juce::Label::backgroundWhenEditingColourId, juce::Colours::white);
        setColour(juce::Label::outlineWhenEditingColourId, juce::Colour(accentStrong));
        setColour(juce::TextButton::buttonColourId, juce::Colours::white.withAlpha(0.48f));
        setColour(juce::TextButton::buttonOnColourId, juce::Colour(accentStrong));
        setColour(juce::TextButton::textColourOffId, juce::Colour(ink));
        setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        setColour(juce::ComboBox::backgroundColourId, juce::Colours::white.withAlpha(0.72f));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(lineBlue).withAlpha(0.85f));
        setColour(juce::ComboBox::textColourId, juce::Colour(ink));
        setColour(juce::ComboBox::arrowColourId, juce::Colour(accentDark));
    }

    juce::Label* createSliderTextBox(juce::Slider& slider) override
    {
        auto* label = juce::LookAndFeel_V4::createSliderTextBox(slider);
        if (label != nullptr)
        {
            label->onEditorShow = [label]
            {
                if (auto* editor = label->getCurrentTextEditor())
                {
                    editor->setColour(juce::TextEditor::highlightColourId, juce::Colour(0xff166fb0));
                    editor->setColour(juce::TextEditor::highlightedTextColourId, juce::Colours::white);
                    editor->setColour(juce::TextEditor::textColourId, juce::Colour(ink));
                    editor->setColour(juce::TextEditor::backgroundColourId, juce::Colours::white);
                    editor->applyColourToAllText(juce::Colour(ink));
                }
            };
        }
        return label;
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider&) override
    {
        // A true circular frosted Aerosound dial, even if the JUCE control is wide.
        // The old ellipse came from shrinking a non-square component rectangle.
        const float cx = x + width * 0.5f;
        const float cy = y + height * 0.5f;
        const float radius = juce::jmax(11.0f,
            juce::jmin(static_cast<float>(width), static_cast<float>(height)) * 0.5f - 4.0f);
        const float dialR = radius * 0.72f;
        const float valueAngle = rotaryStartAngle
            + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        juce::Path track;
        track.addCentredArc(cx, cy, radius - 1.2f, radius - 1.2f, 0.0f,
                            rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff8eb7cd).withAlpha(0.32f));
        g.strokePath(track, juce::PathStrokeType(4.0f,
                     juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path amount;
        amount.addCentredArc(cx, cy, radius - 1.2f, radius - 1.2f, 0.0f,
                             rotaryStartAngle, valueAngle, true);
        g.setColour(juce::Colour(accentStrong));
        g.strokePath(amount, juce::PathStrokeType(4.3f,
                     juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Shadow + soft gradient dial instead of the old flat white oval.
        g.setColour(juce::Colour(0xff5798b9).withAlpha(0.12f));
        g.fillEllipse(cx - dialR + 0.8f, cy - dialR + 2.5f, 2.0f * dialR, 2.0f * dialR);
        juce::ColourGradient face(juce::Colours::white.withAlpha(0.99f),
                                  cx - dialR * 0.50f, cy - dialR * 0.75f,
                                  juce::Colour(0xffd2effd).withAlpha(0.98f),
                                  cx + dialR * 0.60f, cy + dialR * 0.80f, false);
        g.setGradientFill(face);
        g.fillEllipse(cx - dialR, cy - dialR, 2.0f * dialR, 2.0f * dialR);
        g.setColour(juce::Colour(0xff77b4d8).withAlpha(0.74f));
        g.drawEllipse(cx - dialR, cy - dialR, 2.0f * dialR, 2.0f * dialR, 1.1f);

        const float innerR = dialR * 0.78f;
        g.setColour(juce::Colours::white.withAlpha(0.44f));
        g.drawEllipse(cx - innerR, cy - innerR, 2.0f * innerR, 2.0f * innerR, 0.8f);

        // Deliberately high-contrast pointer: angle is visually unambiguous.
        const float startR = dialR * 0.20f;
        const float endR = dialR * 0.73f;
        g.setColour(juce::Colour(accentDark));
        g.drawLine(cx + std::sin(valueAngle) * startR,
                   cy - std::cos(valueAngle) * startR,
                   cx + std::sin(valueAngle) * endR,
                   cy - std::cos(valueAngle) * endR,
                   juce::jmax(2.1f, dialR * 0.13f));
        g.fillEllipse(cx - 2.3f, cy - 2.3f, 4.6f, 4.6f);
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearVertical)
        {
            juce::LookAndFeel_V4::drawLinearSlider(g, x, y, width, height,
                                                   sliderPos, minSliderPos, maxSliderPos,
                                                   style, slider);
            return;
        }

        auto track = juce::Rectangle<float>(x + width * 0.42f, static_cast<float>(y + 4),
                                            width * 0.16f, static_cast<float>(height - 8));
        g.setColour(juce::Colour(meterTrack).withAlpha(0.45f));
        g.fillRoundedRectangle(track, track.getWidth() * 0.5f);

        auto filled = track.withTop(sliderPos);
        g.setColour(juce::Colour(accentStrong));
        g.fillRoundedRectangle(filled, filled.getWidth() * 0.5f);

        if (slider.getName() == "Depth")
        {
            // Read the attenuation directly on the fader thumb.
            auto thumb = juce::Rectangle<float>(juce::jmax(43.0f, width - 5.0f), 26.0f)
                             .withCentre({ track.getCentreX(), sliderPos });
            g.setColour(juce::Colours::white.withAlpha(0.98f));
            g.fillRoundedRectangle(thumb, 8.0f);
            g.setColour(juce::Colour(accentStrong));
            g.drawRoundedRectangle(thumb, 8.0f, 1.5f);
            g.setFont(font(11.5f, juce::Font::bold));
            g.setColour(juce::Colour(ink));
            g.drawFittedText(juce::String(slider.getValue(), 1),
                             thumb.getSmallestIntegerContainer(),
                             juce::Justification::centred, 1);
        }
        else
        {
            const float knob = juce::jmax(15.0f, width * 0.52f);
            auto thumb = juce::Rectangle<float>(knob, knob)
                             .withCentre({ track.getCentreX(), sliderPos });
            g.setColour(juce::Colours::white.withAlpha(0.96f));
            g.fillEllipse(thumb);
            g.setColour(juce::Colour(accentStrong));
            g.drawEllipse(thumb, 1.5f);
        }
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto c = backgroundColour;
        if (shouldDrawButtonAsHighlighted)
            c = c.brighter(0.04f);
        if (shouldDrawButtonAsDown)
            c = c.darker(0.06f);

        auto r = button.getLocalBounds().toFloat().reduced(0.8f);
        g.setColour(c);
        g.fillRoundedRectangle(r, 5.0f);
        g.setColour(juce::Colour(lineBlue).withAlpha(0.82f));
        g.drawRoundedRectangle(r, 5.0f, 1.0f);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool, bool) override
    {
        g.setFont(font(13.0f));
        g.setColour(button.findColour(button.getToggleState()
                                          ? juce::TextButton::textColourOnId
                                          : juce::TextButton::textColourOffId));
        g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(6, 2),
                         juce::Justification::centred, 1);
    }
};

class HeadphoneButton final : public juce::Button
{
public:
    explicit HeadphoneButton(const juce::String& name) : juce::Button(name)
    {
        setClickingTogglesState(true);
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    void paintButton(juce::Graphics& g, bool over, bool down) override
    {
        const bool on = getToggleState();
        auto r = getLocalBounds().toFloat().reduced(1.0f);
        auto bg = on ? juce::Colour(accentStrong)
                     : juce::Colours::white.withAlpha(isEnabled() ? 0.55f : 0.25f);
        if (over && isEnabled())
            bg = bg.brighter(0.05f);
        if (down && isEnabled())
            bg = bg.darker(0.05f);

        g.setColour(bg);
        g.fillRoundedRectangle(r, 5.0f);
        g.setColour(juce::Colour(lineBlue).withAlpha(isEnabled() ? 0.86f : 0.35f));
        g.drawRoundedRectangle(r, 5.0f, 1.0f);

        const auto icon = r.reduced(r.getWidth() * 0.27f, r.getHeight() * 0.24f);
        juce::Path p;
        p.startNewSubPath(icon.getX(), icon.getCentreY());
        p.cubicTo(icon.getX(), icon.getY(),
                  icon.getRight(), icon.getY(),
                  icon.getRight(), icon.getCentreY());
        g.setColour(on ? juce::Colours::white
                       : juce::Colour(accentDark).withAlpha(isEnabled() ? 0.92f : 0.34f));
        g.strokePath(p, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

        const float cupW = icon.getWidth() * 0.18f;
        const float cupH = icon.getHeight() * 0.42f;
        g.fillRoundedRectangle(icon.getX() - cupW * 0.10f, icon.getCentreY() - cupH * 0.06f,
                               cupW, cupH, 2.0f);
        g.fillRoundedRectangle(icon.getRight() - cupW * 0.90f, icon.getCentreY() - cupH * 0.06f,
                               cupW, cupH, 2.0f);
    }
};

class DepthSlider final : public juce::Slider
{
public:
    std::function<void()> onClickWithoutDrag;
    std::function<void()> onDragStarted;

    void setInfinity(bool enabled)
    {
        if (infinity != enabled)
        {
            infinity = enabled;
            repaint();
        }
    }

    void paint(juce::Graphics& g) override
    {
        if (!infinity)
        {
            juce::Slider::paint(g);
            return;
        }

        // Leave the rail visible but hide its thumb: the thumb is now
        // represented by the highlighted infinity button below.
        const auto bounds = getLocalBounds().toFloat();
        const auto rail = juce::Rectangle<float>(
            bounds.getCentreX() - 4.0f, 6.0f, 8.0f, bounds.getHeight() - 12.0f);
        g.setColour(juce::Colour(meterTrack).withAlpha(0.36f));
        g.fillRoundedRectangle(rail, 4.0f);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        downPos = e.position;
        didDrag = false;
        juce::Slider::mouseDown(e);
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        // A real movement immediately leaves infinity, even if the value
        // has not changed yet (e.g. the slider was at the end stop).
        if (!didDrag && e.position.getDistanceFrom(downPos) >= 4.0f)
        {
            didDrag = true;
            if (onDragStarted)
                onDragStarted();
        }
        juce::Slider::mouseDrag(e);
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        const bool click = !didDrag && e.position.getDistanceFrom(downPos) < 4.0f;
        juce::Slider::mouseUp(e);
        if (click && onClickWithoutDrag)
            onClickWithoutDrag();
    }

private:
    juce::Point<float> downPos;
    bool didDrag = false;
    bool infinity = false;
};

class DepthInfinityButton final : public juce::TextButton
{
public:
    explicit DepthInfinityButton(const juce::String& label) : juce::TextButton(label) {}

    void paintButton(juce::Graphics& g, bool hovered, bool down) override
    {
        const auto r = getLocalBounds().toFloat().reduced(1.5f);
        const bool active = getToggleState();

        g.setColour(active ? juce::Colour(0xffe4f7ff)
                           : juce::Colours::white.withAlpha(0.58f));
        g.fillRoundedRectangle(r, 8.0f);
        g.setColour(active ? juce::Colour(accentStrong)
                           : juce::Colour(lineBlue).withAlpha(0.90f));
        g.drawRoundedRectangle(r, 8.0f, active ? 1.8f : 1.1f);

        if (active)
        {
            // The same outlined thumb has dropped off the bottom of the rail.
            const auto thumb = juce::Rectangle<float>(r.getWidth() - 7.0f, r.getHeight() - 5.0f)
                                   .withCentre(r.getCentre());
            g.setColour(juce::Colours::white);
            g.fillRoundedRectangle(thumb, 7.0f);
            g.setColour(juce::Colour(accentStrong));
            g.drawRoundedRectangle(thumb, 7.0f, 1.7f);
        }

        g.setFont(font(12.5f, juce::Font::bold));
        g.setColour(juce::Colour(active ? accentDark : ink)
                        .withAlpha(down ? 0.72f : (hovered ? 1.0f : 0.92f)));
        g.drawText(juce::String::fromUTF8("−∞"), r, juce::Justification::centred);
    }
};
}
