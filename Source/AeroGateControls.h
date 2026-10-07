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
        setColour(juce::TextButton::buttonColourId, juce::Colours::white.withAlpha(0.48f));
        setColour(juce::TextButton::buttonOnColourId, juce::Colour(accentStrong));
        setColour(juce::TextButton::textColourOffId, juce::Colour(ink));
        setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        setColour(juce::ComboBox::backgroundColourId, juce::Colours::white.withAlpha(0.72f));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(lineBlue).withAlpha(0.85f));
        setColour(juce::ComboBox::textColourId, juce::Colour(ink));
        setColour(juce::ComboBox::arrowColourId, juce::Colour(accentDark));
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider&) override
    {
        auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                             static_cast<float>(width), static_cast<float>(height))
                          .reduced(7.0f);
        const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float start = rotaryStartAngle;
        const float end = rotaryEndAngle;
        const float valueAngle = start + sliderPosProportional * (end - start);

        juce::Path track;
        track.addCentredArc(centre.x, centre.y, radius - 3.0f, radius - 3.0f,
                            0.0f, start, end, true);
        g.setColour(juce::Colour(0xff8db8cf).withAlpha(0.28f));
        g.strokePath(track, juce::PathStrokeType(7.5f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

        juce::Path value;
        value.addCentredArc(centre.x, centre.y, radius - 3.0f, radius - 3.0f,
                            0.0f, start, valueAngle, true);
        g.setColour(juce::Colour(accentStrong));
        g.strokePath(value, juce::PathStrokeType(7.5f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

        g.setColour(juce::Colours::white.withAlpha(0.92f));
        g.fillEllipse(bounds.reduced(9.0f));
        g.setColour(juce::Colour(lineBlue).withAlpha(0.62f));
        g.drawEllipse(bounds.reduced(9.0f), 1.0f);

        const float pointerLength = radius * 0.48f;
        const float pointerThickness = 2.0f;
        juce::Path pointer;
        pointer.addRoundedRectangle(-pointerThickness * 0.5f, -radius * 0.48f,
                                    pointerThickness, pointerLength, 1.0f);
        pointer.applyTransform(juce::AffineTransform::rotation(valueAngle)
                                   .translated(centre.x, centre.y));
        g.setColour(juce::Colour(accentDark).withAlpha(0.86f));
        g.fillPath(pointer);
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          const juce::Slider::SliderStyle style, juce::Slider&) override
    {
        if (style != juce::Slider::LinearVertical)
        {
            juce::LookAndFeel_V4::drawLinearSlider(g, x, y, width, height,
                                                   sliderPos, minSliderPos, maxSliderPos,
                                                   style, *static_cast<juce::Slider*>(nullptr));
            return;
        }

        auto track = juce::Rectangle<float>(x + width * 0.42f, static_cast<float>(y + 4),
                                            width * 0.16f, static_cast<float>(height - 8));
        g.setColour(juce::Colour(meterTrack).withAlpha(0.45f));
        g.fillRoundedRectangle(track, track.getWidth() * 0.5f);

        auto filled = track.withTop(sliderPos);
        g.setColour(juce::Colour(accentStrong));
        g.fillRoundedRectangle(filled, filled.getWidth() * 0.5f);

        const float knob = juce::jmax(15.0f, width * 0.52f);
        auto thumb = juce::Rectangle<float>(knob, knob)
                         .withCentre({ track.getCentreX(), sliderPos });
        g.setColour(juce::Colours::white.withAlpha(0.96f));
        g.fillEllipse(thumb);
        g.setColour(juce::Colour(accentStrong));
        g.drawEllipse(thumb, 1.5f);
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

    void mouseDown(const juce::MouseEvent& e) override
    {
        downPos = e.position;
        juce::Slider::mouseDown(e);
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        const bool click = e.position.getDistanceFrom(downPos) < 4.0f;
        juce::Slider::mouseUp(e);
        if (click && onClickWithoutDrag)
            onClickWithoutDrag();
    }

private:
    juce::Point<float> downPos;
};
}
