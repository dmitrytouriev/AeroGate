#include "PluginEditor.h"

#include <cmath>

namespace
{
constexpr auto ink = aerosound::ui::ink;
constexpr auto mutedInk = aerosound::ui::mutedInk;
constexpr auto accentStrong = aerosound::ui::accentStrong;
constexpr auto accentDark = aerosound::ui::accentDark;
constexpr auto lineBlue = aerosound::ui::lineBlue;

juce::Font uiFont(float size, int style = juce::Font::plain)
{
    return aerosound::ui::font(size, style);
}

void drawAerosoundWave(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
{
    static constexpr float heights[] { 0.24f, 0.48f, 0.73f, 0.96f, 0.73f, 0.48f, 0.24f };
    const float centreY = area.getCentreY();
    const float thickness = juce::jmax(4.0f, area.getHeight() * 0.09f);
    const float firstX = area.getX() + area.getWidth() * 0.20f;
    const float lastX = area.getRight() - area.getWidth() * 0.20f;
    const float spacing = (lastX - firstX) / 6.0f;

    g.setColour(colour);

    for (int i = 0; i < 7; ++i)
    {
        const float h = area.getHeight() * heights[i];
        const float cx = firstX + spacing * static_cast<float>(i);
        g.fillRoundedRectangle(cx - thickness * 0.5f, centreY - h * 0.5f,
                               thickness, h, thickness * 0.5f);
    }

    const float tailW = area.getWidth() * 0.13f;
    const float tailH = thickness;
    g.fillRoundedRectangle(area.getX(), centreY - tailH * 0.5f,
                           tailW, tailH, tailH * 0.5f);
    g.fillRoundedRectangle(area.getRight() - tailW, centreY - tailH * 0.5f,
                           tailW, tailH, tailH * 0.5f);
}

float softWave(float x) noexcept
{
    return 0.5f + 0.5f * std::sin(x);
}

// The audio transient stays at a fixed screen position. Lookahead means the
// *detector* begins the envelope before this delayed audio transient arrives,
// so increasing Lookahead moves the whole envelope LEFT, not the hit right.
// Use a shared layout for drawing and curve mouse hit-testing.
struct EnvelopeTimeline
{
    float start = 0.0f;
    float transient = 0.0f;
    float attackEnd = 0.0f;
    float holdEnd = 0.0f;
    float releaseEnd = 0.0f;
};

EnvelopeTimeline makeEnvelopeTimeline(juce::Rectangle<float> chart,
                                      float lookaheadMs, float attackMs,
                                      float holdMs, float releaseMs) noexcept
{
    const float left = chart.getX() + 4.0f;
    // Reserve the right end for the Depth slider, which overlays the plot.
    const float right = chart.getRight() - juce::jmin(64.0f, chart.getWidth() * 0.14f);
    const float transient = left + (right - left) * 0.38f;
    const float postMs = juce::jmax(0.001f, attackMs + holdMs + releaseMs);

    // Scale to the CURRENT envelope, not the maximum possible 40 ms.
    // One shared pixels/ms ratio keeps Attack/Hold/Release and the lead-in
    // perceptually comparable, while the arrival marker stays fixed.
    const float leftSpace = juce::jmax(1.0f, transient - left - 7.0f);
    const float rightSpace = juce::jmax(1.0f, right - transient - 7.0f);
    const float pxPerMs = juce::jmin(
        leftSpace / juce::jmax(0.1f, lookaheadMs),
        rightSpace / postMs);

    const float start = transient - lookaheadMs * pxPerMs;
    const float attackEnd = start + attackMs * pxPerMs;
    const float holdEnd = attackEnd + holdMs * pxPerMs;
    return { start, transient, attackEnd, holdEnd,
             holdEnd + releaseMs * pxPerMs };
}

}

//==============================================================================
SignalFlowComponent::SignalFlowComponent(AeroGateAudioProcessor& p)
    : processor(p)
{
    thresholdValue.setRange(-60.0, 0.0, 0.1);
    closeValue.setRange(-70.0, 0.0, 0.1);

    thresholdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getValueTreeState(), AeroGateAudioProcessor::thresholdParamId, thresholdValue);
    closeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getValueTreeState(), AeroGateAudioProcessor::closeParamId, closeValue);

    const double difference = thresholdValue.getValue() - closeValue.getValue();
    if (difference >= 0.5)
        lastUsableCloseGap = difference;

    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

juce::Point<float> SignalFlowComponent::arcPoint(float radius, float clockDegrees) const noexcept
{
    const auto centre = getLocalBounds().toFloat().getCentre();
    const float radians = juce::degreesToRadians(clockDegrees);
    return { centre.x + radius * std::sin(radians),
             centre.y - radius * std::cos(radians) };
}

// The half-wave and both coloured threshold lines use the same -70..0 dB scale.
// The circle sweeps nearly the full usable waveform height rather than only its top.
float SignalFlowComponent::thresholdAngle(float db) const noexcept
{
    const auto bounds = getLocalBounds().toFloat();
    const auto centre = bounds.getCentre();
    const float radius = juce::jmin(bounds.getHeight() * 0.455f, bounds.getWidth() * 0.145f);
    const float topY = centre.y - radius * std::cos(juce::degreesToRadians(30.0f));
    const float bottomY = centre.y - radius * std::cos(juce::degreesToRadians(150.0f));
    const float norm = juce::jlimit(0.0f, 1.0f, -db / 70.0f);
    const float targetY = topY + norm * (bottomY - topY);
    const float cosine = juce::jlimit(-1.0f, 1.0f, (centre.y - targetY) / radius);
    return juce::radiansToDegrees(std::acos(cosine));
}

float SignalFlowComponent::closeAngle(float db) const noexcept
{
    return 360.0f - thresholdAngle(db);
}

void SignalFlowComponent::drawArc(juce::Graphics& g, float radius, float fromDeg, float toDeg,
                                  juce::Colour colour, float thickness) const
{
    juce::Path p;
    constexpr int steps = 64;

    for (int i = 0; i <= steps; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const float deg = fromDeg + (toDeg - fromDeg) * t;
        const auto pt = arcPoint(radius, deg);
        if (i == 0)
            p.startNewSubPath(pt);
        else
            p.lineTo(pt);
    }

    g.setColour(colour);
    g.strokePath(p, juce::PathStrokeType(thickness,
                                         juce::PathStrokeType::curved,
                                         juce::PathStrokeType::rounded));
}

void SignalFlowComponent::drawWaveform(juce::Graphics& g, juce::Rectangle<float> area,
                                       const std::deque<float>& history, bool newestAtRight) const
{
    if (history.size() < 2 || area.isEmpty())
        return;

    // Render only the upper half. Level is shown on the same dB axis as
    // Threshold/Close, so crossing their horizontal lines is meaningful.
    const float baseline = area.getBottom();
    const float height = area.getHeight();
    const int count = static_cast<int>(history.size());

    const auto levelAt = [&](int index)
    {
        const int i = newestAtRight ? index : (count - 1 - index);
        const float amplitude = juce::jmax(1.0e-7f, history[static_cast<size_t>(i)]);
        const float db = juce::Decibels::gainToDecibels(amplitude, -70.0f);
        return juce::jlimit(0.0f, 1.0f, (db + 70.0f) / 70.0f);
    };

    juce::Path top;
    juce::Path fill;

    for (int i = 0; i < count; ++i)
    {
        const float x = area.getX() + area.getWidth() * static_cast<float>(i)
                                       / static_cast<float>(count - 1);
        const float y = baseline - levelAt(i) * height;
        if (i == 0)
            top.startNewSubPath(x, y);
        else
            top.lineTo(x, y);
    }

    fill = top;
    fill.lineTo(area.getRight(), baseline);
    fill.lineTo(area.getX(), baseline);
    fill.closeSubPath();

    juce::ColourGradient gradient(juce::Colour(0xff3ebbec).withAlpha(0.62f),
                                  area.getX(), area.getY(),
                                  juce::Colour(0xff1593d1).withAlpha(0.23f),
                                  area.getX(), baseline, false);
    g.setGradientFill(gradient);
    g.fillPath(fill);

    g.setColour(juce::Colour(0xff168fcd).withAlpha(0.94f));
    g.strokePath(top, juce::PathStrokeType(1.2f,
                    juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// A pale reflected half continues under the frosted lower controls.
// This is the SAME history as above, not a random or synthesized wave.
void SignalFlowComponent::paintWaveReflection(juce::Graphics& g,
                                              juce::Point<int> editorOrigin) const
{
    const auto bounds = getLocalBounds().toFloat();
    const float s = juce::jmax(0.6f, bounds.getHeight() / 300.0f);
    const auto centre = bounds.getCentre();
    const float radius = juce::jmin(bounds.getHeight() * 0.455f, bounds.getWidth() * 0.145f);
    const float topY = centre.y - radius * std::cos(juce::degreesToRadians(30.0f));
    const float bottomY = centre.y - radius * std::cos(juce::degreesToRadians(150.0f));
    const float halfWidthLeft = juce::jmax(20.0f, centre.x - radius * 1.12f
                                          - bounds.getX() - 34.0f * s);
    const float halfWidthRight = juce::jmax(20.0f, bounds.getRight()
                                           - (centre.x + radius * 1.12f) - 24.0f * s);
    const auto left = juce::Rectangle<float>(editorOrigin.x + bounds.getX() + 24.0f * s,
                                               editorOrigin.y + topY, halfWidthLeft, bottomY - topY);
    const auto right = juce::Rectangle<float>(editorOrigin.x + centre.x + radius * 1.12f,
                                                editorOrigin.y + topY, halfWidthRight, bottomY - topY);

    const auto reflected = [&](const juce::Rectangle<float>& area, const std::deque<float>& history)
    {
        if (history.size() < 2)
            return;

        juce::Path reflectedTop;
        const int count = static_cast<int>(history.size());
        for (int i = 0; i < count; ++i)
        {
            const float amplitude = juce::jmax(1.0e-7f, history[static_cast<size_t>(i)]);
            const float db = juce::Decibels::gainToDecibels(amplitude, -70.0f);
            const float norm = juce::jlimit(0.0f, 1.0f, (db + 70.0f) / 70.0f);
            const float x = area.getX() + area.getWidth() * static_cast<float>(i)
                                           / static_cast<float>(count - 1);
            const float y = area.getBottom() + area.getHeight() * norm;
            if (i == 0)
                reflectedTop.startNewSubPath(x, y);
            else
                reflectedTop.lineTo(x, y);
        }
        juce::Path fill = reflectedTop;
        fill.lineTo(area.getRight(), area.getBottom());
        fill.lineTo(area.getX(), area.getBottom());
        fill.closeSubPath();

        g.setColour(juce::Colour(0xff6ac4ea).withAlpha(0.10f));
        g.fillPath(fill);
        g.setColour(juce::Colour(0xff309ccf).withAlpha(0.12f));
        g.strokePath(reflectedTop, juce::PathStrokeType(1.0f));
    };

    g.saveState();
    const int clipY = editorOrigin.y + getHeight();
    g.reduceClipRegion(juce::Rectangle<int>(0, clipY, g.getClipBounds().getRight(),
                                            juce::jmax(0, g.getClipBounds().getBottom() - clipY)));
    reflected(left, outputHistory);
    reflected(right, inputHistory);
    g.restoreState();
}

void SignalFlowComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const float s = juce::jmax(0.6f, bounds.getHeight() / 300.0f);

    g.setColour(juce::Colour(aerosound::ui::panelFill).withAlpha(0.88f));
    g.fillRoundedRectangle(bounds, aerosound::ui::panelRadius * s);
    g.setColour(juce::Colours::white.withAlpha(0.66f));
    g.drawRoundedRectangle(bounds, aerosound::ui::panelRadius * s, 1.1f * s);

    const auto centre = bounds.getCentre();
    const float radius = juce::jmin(bounds.getHeight() * 0.455f, bounds.getWidth() * 0.145f);
    const float innerRadius = radius * 0.73f;

    const float topY = centre.y - radius * std::cos(juce::degreesToRadians(30.0f));
    const float bottomY = centre.y - radius * std::cos(juce::degreesToRadians(150.0f));
    const auto leftArea = juce::Rectangle<float>(
        bounds.getX() + 24.0f * s, topY,
        juce::jmax(20.0f, centre.x - radius * 1.12f - bounds.getX() - 34.0f * s),
        bottomY - topY);
    const auto rightArea = juce::Rectangle<float>(
        centre.x + radius * 1.12f, topY,
        juce::jmax(20.0f, bounds.getRight() - (centre.x + radius * 1.12f) - 24.0f * s),
        bottomY - topY);

    drawWaveform(g, leftArea, outputHistory, true);
    drawWaveform(g, rightArea, inputHistory, true);

    g.setColour(juce::Colour(ink));
    g.setFont(uiFont(19.0f * s, juce::Font::bold));
    g.drawText("OUTPUT", leftArea.withY(bounds.getY() + 16.0f * s).withHeight(28.0f * s),
               juce::Justification::centred);
    g.drawText("INPUT", rightArea.withY(bounds.getY() + 16.0f * s).withHeight(28.0f * s),
               juce::Justification::centred);

    const float threshold = static_cast<float>(thresholdValue.getValue());
    const bool closeEnabled = processor.getValueTreeState().getRawParameterValue(
        AeroGateAudioProcessor::closeEnabledParamId)->load() >= 0.5f;
    const float storedClose = juce::jmin(static_cast<float>(closeValue.getValue()), threshold);
    const float close = closeEnabled ? storedClose : threshold;
    const auto thresholdHandle = arcPoint(radius, thresholdAngle(threshold));
    const auto closeHandle = arcPoint(radius, closeAngle(close));

    const float dashPattern[] { 7.0f * s, 4.0f * s };

    g.setColour(juce::Colour(accentStrong).withAlpha(0.96f));
    juce::Line<float> thresholdLeft(bounds.getX() + 2.0f * s, thresholdHandle.y,
                                    centre.x - radius * 0.97f, thresholdHandle.y);
    juce::Line<float> thresholdRight(centre.x + radius * 0.97f, thresholdHandle.y,
                                     bounds.getRight() - 2.0f * s, thresholdHandle.y);
    g.drawDashedLine(thresholdLeft, dashPattern, 2, 1.5f * s);
    g.drawDashedLine(thresholdRight, dashPattern, 2, 1.5f * s);

    if (closeEnabled)
    {
    g.setColour(juce::Colour(aerosound::ui::meterOrange).withAlpha(0.98f));
    juce::Line<float> closeLeft(bounds.getX() + 2.0f * s, closeHandle.y,
                                centre.x - radius * 0.97f, closeHandle.y);
    juce::Line<float> closeRight(centre.x + radius * 0.97f, closeHandle.y,
                                 bounds.getRight() - 2.0f * s, closeHandle.y);
    g.drawDashedLine(closeLeft, dashPattern, 2, 1.5f * s);
    g.drawDashedLine(closeRight, dashPattern, 2, 1.5f * s);
    }

    g.setColour(juce::Colours::white.withAlpha(0.95f));
    g.fillEllipse(centre.x - innerRadius, centre.y - innerRadius,
                  innerRadius * 2.0f, innerRadius * 2.0f);
    g.setColour(juce::Colour(lineBlue).withAlpha(0.56f));
    g.drawEllipse(centre.x - innerRadius, centre.y - innerRadius,
                  innerRadius * 2.0f, innerRadius * 2.0f, 1.2f * s);

    drawArc(g, radius, 150.0f, 30.0f, juce::Colour(0xff91b4c7).withAlpha(0.34f), 13.0f * s);
    drawArc(g, radius, 150.0f, thresholdAngle(threshold), juce::Colour(accentStrong), 13.0f * s);

    drawArc(g, radius, 210.0f, 330.0f, juce::Colour(0xff91b4c7).withAlpha(0.34f), 13.0f * s);
    if (closeEnabled)
        drawArc(g, radius, 210.0f, closeAngle(close),
                juce::Colour(aerosound::ui::meterOrange), 13.0f * s);

    auto drawHandle = [&](juce::Point<float> pt, juce::Colour c)
    {
        const float d = 23.0f * s;
        g.setColour(juce::Colours::white.withAlpha(0.98f));
        g.fillEllipse(pt.x - d * 0.5f, pt.y - d * 0.5f, d, d);
        g.setColour(c);
        g.drawEllipse(pt.x - d * 0.5f, pt.y - d * 0.5f, d, d, 2.0f * s);
    };

    drawHandle(thresholdHandle, juce::Colour(accentStrong));
    if (closeEnabled)
        drawHandle(closeHandle, juce::Colour(aerosound::ui::meterOrange));

    const auto logoArea = juce::Rectangle<float>(innerRadius * 1.15f, innerRadius * 0.48f)
                              .withCentre({ centre.x, centre.y - innerRadius * 0.32f });
    drawAerosoundWave(g, logoArea, juce::Colour(accentStrong));

    g.setFont(uiFont(13.0f * s, juce::Font::bold));
    g.setColour(juce::Colour(mutedInk));
    g.drawText("THRESHOLD",
               juce::Rectangle<float>(centre.x - innerRadius, centre.y + 20.0f * s,
                                      innerRadius * 2.0f, 22.0f * s),
               juce::Justification::centred);

    g.setFont(uiFont(21.0f * s, juce::Font::bold));
    g.setColour(juce::Colour(0xff1539a4));
    g.drawText(juce::String(threshold, 1) + " dB",
               juce::Rectangle<float>(centre.x - innerRadius, centre.y + 42.0f * s,
                                      innerRadius * 2.0f, 30.0f * s),
               juce::Justification::centred);

    // Neutral, familiar button: white outline when active, grey when disabled.
    const auto closeButton = juce::Rectangle<float>(
        109.0f * s, 27.0f * s)
        .withCentre({ centre.x, centre.y + 83.0f * s });
    g.setColour(closeEnabled ? juce::Colours::white.withAlpha(0.54f)
                             : juce::Colour(0xffbecbd4).withAlpha(0.48f));
    g.fillRoundedRectangle(closeButton, 7.0f * s);
    g.setColour(closeEnabled ? juce::Colour(0xff90c9ea)
                             : juce::Colour(0xffa3afb9));
    g.drawRoundedRectangle(closeButton, 7.0f * s, 2.0f * s);
    g.setFont(uiFont(12.5f * s, juce::Font::bold));
    g.setColour(closeEnabled ? juce::Colour(ink)
                             : juce::Colour(mutedInk).withAlpha(0.56f));
    g.drawText("CLOSE", closeButton, juce::Justification::centred);

    g.setFont(uiFont(18.0f * s, juce::Font::bold));
    g.setColour(closeEnabled ? juce::Colour(0xff27366d)
                             : juce::Colour(mutedInk).withAlpha(0.60f));
    g.drawText(juce::String(close, 1) + " dB",
               juce::Rectangle<float>(centre.x - innerRadius, centre.y + 92.0f * s,
                                      innerRadius * 2.0f, 26.0f * s),
               juce::Justification::centred);
}

void SignalFlowComponent::setCloseEnabled(bool enabled)
{
    auto& state = processor.getValueTreeState();
    const bool currentlyEnabled = state.getRawParameterValue(
        AeroGateAudioProcessor::closeEnabledParamId)->load() >= 0.5f;

    if (currentlyEnabled == enabled)
        return;

    if (currentlyEnabled)
    {
        const double gap = thresholdValue.getValue() - closeValue.getValue();
        if (gap >= 0.5)
            lastUsableCloseGap = gap;
    }
    else if (enabled)
    {
        // Recover the previously audible hysteresis, never reactivate at equality.
        const double threshold = thresholdValue.getValue();
        const double desiredClose = juce::jlimit(-70.0, threshold - 0.1,
                                                 threshold - lastUsableCloseGap);
        closeValue.setValue(desiredClose, juce::sendNotificationSync);
    }

    if (auto* param = state.getParameter(AeroGateAudioProcessor::closeEnabledParamId))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost(enabled ? 1.0f : 0.0f);
        param->endChangeGesture();
    }
    repaint();
}

void SignalFlowComponent::syncCloseState()
{
    const auto& state = processor.getValueTreeState();
    const bool enabled = state.getRawParameterValue(
        AeroGateAudioProcessor::closeEnabledParamId)->load() >= 0.5f;
    const double threshold = thresholdValue.getValue();
    const double close = closeValue.getValue();

    // Host automation or typed values may also make CLOSE meet THRESHOLD.
    if (enabled && close >= threshold - 0.05)
    {
        setCloseEnabled(false);
        closeValue.setValue(juce::jmax(-70.0, threshold - lastUsableCloseGap),
                            juce::sendNotificationSync);
    }
}

void SignalFlowComponent::mouseDown(const juce::MouseEvent& e)
{
    const auto bounds = getLocalBounds().toFloat();
    const float s = juce::jmax(0.6f, bounds.getHeight() / 300.0f);
    const auto centre = bounds.getCentre();

    const auto closeButton = juce::Rectangle<float>(109.0f * s, 27.0f * s)
        .withCentre({ centre.x, centre.y + 83.0f * s });

    if (closeButton.contains(e.position))
    {
        const bool enabled = processor.getValueTreeState().getRawParameterValue(
            AeroGateAudioProcessor::closeEnabledParamId)->load() >= 0.5f;
        setCloseEnabled(!enabled);
        dragTarget = DragTarget::none;
        return;
    }

    dragTarget = e.position.x >= centre.x ? DragTarget::threshold : DragTarget::close;
    dragStartY = e.position.y;
    processor.beginHistoryGesture();

    if (dragTarget == DragTarget::threshold)
    {
        dragStartValue = thresholdValue.getValue();
        thresholdCloseGap = juce::jmax(0.0, thresholdValue.getValue() - closeValue.getValue());
        if (thresholdCloseGap >= 0.5)
            lastUsableCloseGap = thresholdCloseGap;
    }
    else
    {
        const bool enabled = processor.getValueTreeState().getRawParameterValue(
            AeroGateAudioProcessor::closeEnabledParamId)->load() >= 0.5f;
        if (!enabled)
            setCloseEnabled(true);

        dragStartValue = closeValue.getValue();
        thresholdCloseGap = juce::jmax(0.0, thresholdValue.getValue() - dragStartValue);
        if (thresholdCloseGap >= 0.5)
            lastUsableCloseGap = thresholdCloseGap;
    }
}

void SignalFlowComponent::mouseDrag(const juce::MouseEvent& e)
{
    const double deltaDb = static_cast<double>(dragStartY - e.position.y) * 0.18;

    if (dragTarget == DragTarget::threshold)
    {
        const double newThreshold = juce::jlimit(-60.0, 0.0, dragStartValue + deltaDb);
        const bool enabled = processor.getValueTreeState().getRawParameterValue(
            AeroGateAudioProcessor::closeEnabledParamId)->load() >= 0.5f;

        // THRESHOLD and CLOSE form a linked pair: moving THRESHOLD keeps
        // their dB difference (subject to the -70 dB CLOSE lower limit).
        // Dragging CLOSE itself is how the user changes the hysteresis and
        // can switch CLOSE off by making the two levels equal.
        const double gap = enabled ? thresholdCloseGap : lastUsableCloseGap;
        const double newClose = juce::jlimit(-70.0, newThreshold,
                                             newThreshold - gap);
        // Move CLOSE first when raising the threshold, and THRESHOLD first
        // when lowering it, so their ordering holds even during callbacks.
        if (newThreshold >= thresholdValue.getValue())
        {
            closeValue.setValue(newClose, juce::sendNotificationSync);
            thresholdValue.setValue(newThreshold, juce::sendNotificationSync);
        }
        else
        {
            thresholdValue.setValue(newThreshold, juce::sendNotificationSync);
            closeValue.setValue(newClose, juce::sendNotificationSync);
        }
    }
    else if (dragTarget == DragTarget::close)
    {
        const double threshold = thresholdValue.getValue();
        const double candidate = juce::jlimit(-70.0, threshold, dragStartValue + deltaDb);
        const bool enabled = processor.getValueTreeState().getRawParameterValue(
            AeroGateAudioProcessor::closeEnabledParamId)->load() >= 0.5f;

        if (candidate >= threshold - 0.05)
        {
            if (enabled)
            {
                // Preserve the last distinct gap for a later click on CLOSE.
                lastUsableCloseGap = juce::jmax(0.5, thresholdCloseGap);
                setCloseEnabled(false);
                closeValue.setValue(juce::jmax(-70.0, threshold - lastUsableCloseGap),
                                    juce::sendNotificationSync);
            }
        }
        else if (candidate < threshold - 0.5)
        {
            // Dragging back down can immediately reactivate hysteresis.
            closeValue.setValue(candidate, juce::sendNotificationSync);
            if (!enabled)
                setCloseEnabled(true);
        }
    }

    repaint();
}

void SignalFlowComponent::mouseUp(const juce::MouseEvent&)
{
    if (dragTarget != DragTarget::none)
        processor.endHistoryGesture();
    dragTarget = DragTarget::none;
}

void SignalFlowComponent::mouseDoubleClick(const juce::MouseEvent&)
{
    thresholdValue.setValue(-24.0, juce::sendNotificationSync);
    closeValue.setValue(-30.0, juce::sendNotificationSync);
    repaint();
}

void SignalFlowComponent::pushFrame(const AeroGateAudioProcessor::ScopeFrame& f)
{
    constexpr size_t maxHistory = 400;

    inputHistory.push_back(juce::jlimit(0.0f, 1.0f, f.input));
    outputHistory.push_back(juce::jlimit(0.0f, 1.0f, f.output));

    while (inputHistory.size() > maxHistory)
        inputHistory.pop_front();
    while (outputHistory.size() > maxHistory)
        outputHistory.pop_front();

    repaint();
}

//==============================================================================
GateEnvelopePreview::GateEnvelopePreview(AeroGateAudioProcessor& p) : processor(p)
{
    setInterceptsMouseClicks(true, false);
    setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
}

void GateEnvelopePreview::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced(2.0f);
    g.setColour(juce::Colours::white.withAlpha(0.32f));
    g.fillRoundedRectangle(r, 8.0f);
    g.setColour(juce::Colour(lineBlue).withAlpha(0.52f));
    g.drawRoundedRectangle(r, 8.0f, 1.0f);

    auto chart = r.reduced(12.0f, 10.0f);
    chart.removeFromTop(17.0f);

    const float lookahead = processor.getValueTreeState().getRawParameterValue(AeroGateAudioProcessor::lookaheadParamId)->load();
    const float attack = processor.getValueTreeState().getRawParameterValue(AeroGateAudioProcessor::attackParamId)->load();
    const float hold = processor.getValueTreeState().getRawParameterValue(AeroGateAudioProcessor::holdParamId)->load();
    const float release = processor.getValueTreeState().getRawParameterValue(AeroGateAudioProcessor::releaseParamId)->load();
    const float depth = processor.getValueTreeState().getRawParameterValue(AeroGateAudioProcessor::depthParamId)->load();
    const bool depthInf = processor.getValueTreeState().getRawParameterValue(AeroGateAudioProcessor::depthInfParamId)->load() >= 0.5f;
    const bool ducking = processor.getValueTreeState().getRawParameterValue(AeroGateAudioProcessor::modeParamId)->load() >= 0.5f;
    const int attackShape = juce::jlimit(0, 2, juce::roundToInt(processor.getValueTreeState().getRawParameterValue(AeroGateAudioProcessor::attackCurveParamId)->load()));
    const int releaseShape = juce::jlimit(0, 2, juce::roundToInt(processor.getValueTreeState().getRawParameterValue(AeroGateAudioProcessor::releaseCurveParamId)->load()));

    // Equal milliseconds share one scale on both sides of the fixed transient.
    // Lookahead moves the envelope backwards relative to arriving audio.
    const auto timeline = makeEnvelopeTimeline(chart, lookahead, attack, hold, release);
    const float x0 = chart.getX();
    const float x1 = timeline.start;
    const float x2 = timeline.attackEnd;
    const float x3 = timeline.holdEnd;
    const float x4 = timeline.releaseEnd;

    const float topY = chart.getY() + 7.0f;
    const float bottomY = chart.getBottom() - 4.0f;
    const float depthNorm = depthInf ? 0.0f : juce::jlimit(0.0f, 1.0f, (depth + 80.0f) / 80.0f);
    const float depthY = bottomY - depthNorm * (bottomY - topY);
    const float openY = topY;

    const float closedY = depthInf ? bottomY : depthY;
    const float idleY = ducking ? openY : closedY;
    const float activeY = ducking ? closedY : openY;

    // Highlight the time by which the detector leads the arriving audio.
    // This may overlap Attack, Hold or Release: the audible transient is fixed.
    const juce::Rectangle<float> lookaheadArea(x1, chart.getY(),
        juce::jmax(0.0f, timeline.transient - x1), chart.getHeight());
    if (lookaheadArea.getWidth() > 0.0f)
    {
        g.saveState();
        g.reduceClipRegion(lookaheadArea.getSmallestIntegerContainer());
        g.setColour(juce::Colour(0xff85a8bb).withAlpha(0.12f));
        g.fillRect(lookaheadArea);
        g.setColour(juce::Colour(0xff85a8bb).withAlpha(0.25f));
        for (float x = x1 - chart.getHeight(); x < timeline.transient; x += 8.0f)
            g.drawLine(x, chart.getBottom(), x + chart.getHeight(), chart.getY(), 0.8f);
        g.restoreState();
    }

    g.setColour(juce::Colour(lineBlue).withAlpha(0.38f));
    for (float x : { x1, x2, x3, x4 })
        g.drawVerticalLine(juce::roundToInt(x), chart.getY(), chart.getBottom());

    juce::Path env;
    env.startNewSubPath(x0, idleY);
    env.lineTo(x1, idleY);
    // Exactly the same easedProgress curve used in the DSP.
    const auto appendCurve = [&](float left, float right, float from, float to, int shape)
    {
        constexpr int slices = 40;
        for (int i = 1; i <= slices; ++i)
        {
            const float t = static_cast<float>(i) / slices;
            const float eased = aerogate::envelope::easedProgress(t, shape);
            env.lineTo(left + t * (right - left), from + eased * (to - from));
        }
    };
    appendCurve(x1, x2, idleY, activeY, attackShape);
    env.lineTo(x3, activeY);
    appendCurve(x3, x4, activeY, idleY, releaseShape);
    // Continue the settled envelope to the plot edge in BOTH modes.
    env.lineTo(chart.getRight(), idleY);

    juce::Path fill = env;
    fill.lineTo(chart.getRight(), bottomY);
    fill.lineTo(x0, bottomY);
    fill.closeSubPath();

    g.setColour(juce::Colour(0xff64c4ed).withAlpha(0.22f));
    g.fillPath(fill);
    g.setColour(juce::Colour(accentStrong));
    g.strokePath(env, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));

    // Stationary arriving-audio marker. It does NOT travel when Lookahead
    // changes; the detector-controlled envelope travels relative to it.
    g.setColour(juce::Colour(aerosound::ui::meterOrange).withAlpha(0.90f));
    g.drawVerticalLine(juce::roundToInt(timeline.transient),
                       chart.getY(), chart.getBottom());
    juce::Path impulse;
    impulse.addTriangle(timeline.transient - 4.5f, chart.getY() + 1.0f,
                        timeline.transient + 4.5f, chart.getY() + 1.0f,
                        timeline.transient, chart.getY() + 9.0f);
    g.fillPath(impulse);
    g.setFont(uiFont(10.0f, juce::Font::bold));
    g.drawText("HIT", juce::Rectangle<float>(timeline.transient - 19.0f,
               chart.getY() - 17.0f, 38.0f, 15.0f), juce::Justification::centred);

    // Small white grab points help discover direct manipulation.
    const auto handle = [&](float xA, float xB, float yA, float yB, int shape, bool draggingThis)
    {
        if (xB - xA < 7.0f) return;
        const float x = (xA + xB) * 0.5f;
        const float y = yA + aerogate::envelope::easedProgress(0.5f, shape) * (yB - yA);
        g.setColour(juce::Colours::white.withAlpha(0.98f));
        g.fillEllipse(x - 4.0f, y - 4.0f, 8.0f, 8.0f);
        g.setColour(juce::Colour(draggingThis ? accentDark : accentStrong));
        g.drawEllipse(x - 4.0f, y - 4.0f, 8.0f, 8.0f, 1.5f);
    };
    handle(x1, x2, idleY, activeY, attackShape, dragging == Segment::attack);
    handle(x3, x4, activeY, idleY, releaseShape, dragging == Segment::release);

    // Fixed parameter headings remain legible regardless of interval durations.
    // Parent editor labels would otherwise be hidden behind this full-panel child.
    g.setFont(uiFont(11.5f));
    g.setColour(juce::Colour(mutedInk));
    const float labelScaleX = getWidth() / 558.0f;
    const float labelScaleY = getHeight() / 220.0f;
    auto captionRect = [labelScaleX, labelScaleY](float x, float y, float w, float h)
    {
        return juce::Rectangle<float>(x * labelScaleX, y * labelScaleY,
                                      w * labelScaleX, h * labelScaleY);
    };
    g.drawText("LOOKAHEAD", captionRect(8, 126, 102, 18), juce::Justification::centred);
    g.drawText("ATTACK", captionRect(123, 126, 102, 18), juce::Justification::centred);
    g.drawText("HOLD", captionRect(238, 126, 102, 18), juce::Justification::centred);
    g.drawText("RELEASE", captionRect(353, 126, 102, 18), juce::Justification::centred);
    g.drawText("DEPTH", captionRect(483, 3, 64, 18), juce::Justification::centred);

    // Current Depth value is rendered on the slider's moving thumb.
    // In -inf the thumb is parked in the button directly below the rail.
}

void GateEnvelopePreview::mouseDown(const juce::MouseEvent& e)
{
    dragging = Segment::none;
    dragParameter = nullptr;
    gestureStarted = false;
    auto chart = getLocalBounds().toFloat().reduced(2.0f).reduced(12.0f, 10.0f);
    chart.removeFromTop(17.0f);
    if (!chart.contains(e.position)) return;

    const auto& state = processor.getValueTreeState();
    const float ahead = state.getRawParameterValue(AeroGateAudioProcessor::lookaheadParamId)->load();
    const float attack = state.getRawParameterValue(AeroGateAudioProcessor::attackParamId)->load();
    const float hold = state.getRawParameterValue(AeroGateAudioProcessor::holdParamId)->load();
    const float release = state.getRawParameterValue(AeroGateAudioProcessor::releaseParamId)->load();
    const auto timeline = makeEnvelopeTimeline(chart, ahead, attack, hold, release);
    const bool ducking = state.getRawParameterValue(
        AeroGateAudioProcessor::modeParamId)->load() >= 0.5f;
    const bool depthInf = state.getRawParameterValue(
        AeroGateAudioProcessor::depthInfParamId)->load() >= 0.5f;
    const float depth = state.getRawParameterValue(AeroGateAudioProcessor::depthParamId)->load();
    const float top = chart.getY() + 7.0f;
    const float bottom = chart.getBottom() - 4.0f;
    const float depthNorm = depthInf ? 0.0f
        : juce::jlimit(0.0f, 1.0f, (depth + 80.0f) / 80.0f);
    const float closed = depthInf ? bottom : bottom - depthNorm * (bottom - top);
    const float idle = ducking ? top : closed;
    const float active = ducking ? closed : top;
    const int attackShape = juce::jlimit(0, 2, juce::roundToInt(
        state.getRawParameterValue(AeroGateAudioProcessor::attackCurveParamId)->load()));
    const int releaseShape = juce::jlimit(0, 2, juce::roundToInt(
        state.getRawParameterValue(AeroGateAudioProcessor::releaseCurveParamId)->load()));

    // Any point ALONG the drawn curve is draggable. Do not activate a curve
    // when clicking empty space in its horizontal range.
    const auto nearCurve = [&](float fromX, float toX, float fromY, float toY, int shape)
    {
        if (toX <= fromX || e.position.x < fromX - 9.0f || e.position.x > toX + 9.0f)
            return false;
        const float fraction = juce::jlimit(0.0f, 1.0f,
            (e.position.x - fromX) / (toX - fromX));
        const float y = fromY + aerogate::envelope::easedProgress(fraction, shape)
            * (toY - fromY);
        return std::abs(e.position.y - y) <= 12.0f;
    };
    const bool nearAttack = nearCurve(timeline.start, timeline.attackEnd,
                                      idle, active, attackShape);
    const bool nearRelease = nearCurve(timeline.holdEnd, timeline.releaseEnd,
                                       active, idle, releaseShape);
    if (nearAttack && (!nearRelease
        || std::abs(e.position.x - (timeline.start + timeline.attackEnd) * 0.5f)
            < std::abs(e.position.x - (timeline.holdEnd + timeline.releaseEnd) * 0.5f)))
        dragging = Segment::attack;
    else if (nearRelease)
        dragging = Segment::release;

    if (dragging != Segment::none)
    {
        processor.beginHistoryGesture();
        dragParameter = state.getParameter(dragging == Segment::attack
            ? AeroGateAudioProcessor::attackCurveParamId
            : AeroGateAudioProcessor::releaseCurveParamId);
        mouseStartY = e.position.y;
        currentCurveShape = juce::jlimit(0, 2, juce::roundToInt(
            state.getRawParameterValue(dragging == Segment::attack
                ? AeroGateAudioProcessor::attackCurveParamId
                : AeroGateAudioProcessor::releaseCurveParamId)->load()));
    }
}

void GateEnvelopePreview::mouseDrag(const juce::MouseEvent& e)
{
    if (dragParameter == nullptr) return;

    // Move one state at a time, starting from the CURRENT shape. Each change
    // needs a fresh 24-pixel drag; changing direction has its own dead zone.
    constexpr float stepPixels = 24.0f;
    const float dy = e.position.y - mouseStartY;
    if (std::abs(dy) < stepPixels) return;

    const bool ducking = processor.getValueTreeState().getRawParameterValue(
        AeroGateAudioProcessor::modeParamId)->load() >= 0.5f;
    const bool risingOnGraph = (dragging == Segment::attack) != ducking;
    const int upDirection = risingOnGraph ? -1 : 1; // toward Fast on a rising segment
    const int newShape = juce::jlimit(0, 2,
        currentCurveShape + (dy < 0.0f ? upDirection : -upDirection));

    if (newShape != currentCurveShape)
    {
        if (!gestureStarted)
        {
            dragParameter->beginChangeGesture();
            gestureStarted = true;
        }

        currentCurveShape = newShape;
        dragParameter->setValueNotifyingHost(
            dragParameter->convertTo0to1(static_cast<float>(currentCurveShape)));
        repaint();
    }

    // Even if we hit an endpoint, a new gesture is needed for another step.
    mouseStartY = e.position.y;
}

void GateEnvelopePreview::mouseUp(const juce::MouseEvent&)
{
    if (gestureStarted && dragParameter != nullptr)
        dragParameter->endChangeGesture();
    if (dragging != Segment::none)
        processor.endHistoryGesture();
    gestureStarted = false;
    dragParameter = nullptr;
    dragging = Segment::none;
    repaint();
}

//==============================================================================

DetectorScope::DetectorScope(AeroGateAudioProcessor& p) : processor(p)
{
    setInterceptsMouseClicks(false, false);
}

void DetectorScope::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(juce::Colours::white.withAlpha(0.30f));
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(juce::Colour(lineBlue).withAlpha(0.50f));
    g.drawRoundedRectangle(bounds, 8.0f, 1.0f);

    // Full-panel EQ: labels are painted inside the child, above floating knobs.
    g.setColour(juce::Colour(mutedInk));
    g.setFont(uiFont(11.5f));
    const float labelScaleX = getWidth() / 410.0f;
    const float labelScaleY = getHeight() / 220.0f;
    g.drawText("HPF", juce::Rectangle<float>(6.0f * labelScaleX, 88.0f * labelScaleY,
                                            95.0f * labelScaleX, 18.0f * labelScaleY),
               juce::Justification::centred);
    g.drawText("LPF", juce::Rectangle<float>(303.0f * labelScaleX, 88.0f * labelScaleY,
                                            95.0f * labelScaleX, 18.0f * labelScaleY),
               juce::Justification::centred);

    auto plot = bounds.reduced(9.0f, 3.0f);
    plot.removeFromTop(15.0f);
    plot.removeFromBottom(11.0f);
    if (plot.getWidth() <= 5.0f || plot.getHeight() <= 5.0f)
        return;

    const auto& state = processor.getValueTreeState();
    const float lpHz = state.getRawParameterValue(AeroGateAudioProcessor::lpfParamId)->load();
    const float hpHz = juce::jmin(
        state.getRawParameterValue(AeroGateAudioProcessor::hpfParamId)->load(), lpHz);
    const int hpSlope = juce::jlimit(0, 5, juce::roundToInt(
        state.getRawParameterValue(AeroGateAudioProcessor::hpfSlopeParamId)->load()));
    const int lpSlope = juce::jlimit(0, 5, juce::roundToInt(
        state.getRawParameterValue(AeroGateAudioProcessor::lpfSlopeParamId)->load()));
    constexpr int orders[] { 2, 3, 4, 6, 8, 16 };
    const bool hpOff = hpHz <= 20.05f;
    const bool lpOff = lpHz >= 19999.0f;
    // At equal cutoffs the two filters meet; do not show an empty band.
    const bool empty = false;

    const auto xForHz = [&](float hz)
    {
        const float fraction = std::log(juce::jlimit(20.0f, 20000.0f, hz) / 20.0f)
                               / std::log(1000.0f);
        return plot.getX() + fraction * plot.getWidth();
    };

    // Logarithmic frequency axis: 20 Hz -> 20 kHz.
    g.setColour(juce::Colour(lineBlue).withAlpha(0.25f));
    for (float hz : { 20.0f, 100.0f, 1000.0f, 10000.0f, 20000.0f })
    {
        const float x = xForHz(hz);
        g.drawVerticalLine(juce::roundToInt(x), plot.getY(), plot.getBottom());
    }
    for (float db : { 0.0f, -24.0f, -48.0f, -72.0f })
    {
        const float y = plot.getY() + (-db / 72.0f) * plot.getHeight();
        g.drawHorizontalLine(juce::roundToInt(y), plot.getX(), plot.getRight());
    }

    const auto responseAt = [&](float hz)
    {
        if (empty)
            return -72.0f;

        // Butterworth magnitude approximation of the DSP cascades.
        // The diagram shows the theoretical EQ curve, not an FFT spectrum.
        const float highRatio = hpHz / hz;
        const float lowRatio = hz / lpHz;
        // Match the DSP's TRUE bypass states at the extreme cutoff positions.
        const double hpPower = hpOff ? 0.0 :
            std::pow(static_cast<double>(highRatio), 2 * orders[hpSlope]);
        const double lpPower = lpOff ? 0.0 :
            std::pow(static_cast<double>(lowRatio), 2 * orders[lpSlope]);
        const double attenuation = -10.0 * std::log10(1.0 + hpPower)
                                   -10.0 * std::log10(1.0 + lpPower);
        return juce::jlimit(-72.0f, 0.0f, static_cast<float>(attenuation));
    };

    juce::Path response;
    constexpr int samples = 240;
    for (int i = 0; i <= samples; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(samples);
        const float hz = 20.0f * std::pow(1000.0f, t);
        const float db = responseAt(hz);
        const float x = plot.getX() + t * plot.getWidth();
        const float y = plot.getY() + (-db / 72.0f) * plot.getHeight();
        if (i == 0)
            response.startNewSubPath(x, y);
        else
            response.lineTo(x, y);
    }

    juce::Path area = response;
    area.lineTo(plot.getRight(), plot.getBottom());
    area.lineTo(plot.getX(), plot.getBottom());
    area.closeSubPath();

    g.setColour(juce::Colour(0xff2aafdf).withAlpha(empty ? 0.08f : 0.25f));
    g.fillPath(area);
    g.setColour(juce::Colour(accentStrong).withAlpha(empty ? 0.36f : 0.95f));
    g.strokePath(response, juce::PathStrokeType(1.8f,
                  juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Coloured cutoff guides: blue HPF and orange LPF.
    if (!hpOff)
    {
        g.setColour(juce::Colour(accentStrong).withAlpha(0.68f));
        g.drawVerticalLine(juce::roundToInt(xForHz(hpHz)), plot.getY(), plot.getBottom());
    }
    if (!lpOff)
    {
        g.setColour(juce::Colour(aerosound::ui::meterOrange).withAlpha(0.80f));
        g.drawVerticalLine(juce::roundToInt(xForHz(lpHz)), plot.getY(), plot.getBottom());
    }

    g.setFont(uiFont(9.0f));
    g.setColour(juce::Colour(mutedInk));
    auto labels = bounds.reduced(8.0f, 1.0f).removeFromBottom(12.0f);
    const float w = labels.getWidth();
    g.drawText("20", labels.withWidth(w * 0.15f), juce::Justification::centredLeft);
    g.drawText("100", labels.withX(labels.getX() + w * 0.17f).withWidth(w * 0.20f),
               juce::Justification::centred);
    g.drawText("1k", labels.withX(labels.getX() + w * 0.42f).withWidth(w * 0.18f),
               juce::Justification::centred);
    g.drawText("10k", labels.withX(labels.getX() + w * 0.72f).withWidth(w * 0.17f),
               juce::Justification::centred);
    g.drawText("20k", labels.withX(labels.getX() + w * 0.85f).withWidth(w * 0.15f),
               juce::Justification::centredRight);

    if (empty)
    {
        g.setColour(juce::Colour(ink).withAlpha(0.84f));
        g.setFont(uiFont(10.0f, juce::Font::bold));
        g.drawText("NO PASSBAND", plot, juce::Justification::centred);
    }
}

//==============================================================================
void AeroGateAudioProcessorEditor::BypassOverlay::setSnapshot(juce::Image image)
{
    snapshot = std::move(image);
    repaint();
}

void AeroGateAudioProcessorEditor::BypassOverlay::paint(juce::Graphics& g)
{
    if (snapshot.isValid())
        g.drawImageAt(snapshot, 0, 0);

    g.setColour(juce::Colour(0xffc4c8cb).withAlpha(0.14f));
    g.fillAll();
}

//==============================================================================
AeroGateAudioProcessorEditor::PopupOverlay::PopupOverlay(bool donationPopup)
    : donation(donationPopup)
{
    setInterceptsMouseClicks(donation, donation);
    setWantsKeyboardFocus(false);
}

void AeroGateAudioProcessorEditor::PopupOverlay::setQrImage(juce::Image image)
{
    qrImage = std::move(image);
    repaint();
}

void AeroGateAudioProcessorEditor::PopupOverlay::paint(juce::Graphics& g)
{
    const float sx = getWidth() / 1100.0f;
    const float sy = getHeight() / 760.0f;
    const float s = juce::jmin(sx, sy);

    if (!donation)
    {
        // Aerosound AutoTrim Help: full-screen wash and contextual yellow cards.
        g.setColour(juce::Colours::white.withAlpha(0.46f));
        g.fillAll();

        const auto rect = [sx, sy](float x, float y, float w, float h)
        {
            return juce::Rectangle<int>(juce::roundToInt(x * sx),
                                        juce::roundToInt(y * sy),
                                        juce::roundToInt(w * sx),
                                        juce::roundToInt(h * sy));
        };

        const auto drawCard = [&](juce::Rectangle<int> r, const juce::String& message)
        {
            g.setColour(juce::Colours::black.withAlpha(0.11f));
            g.fillRoundedRectangle(r.toFloat().translated(2.0f * s, 3.0f * s), 7.0f * s);
            g.setColour(juce::Colour(0xffffefad));
            g.fillRoundedRectangle(r.toFloat(), 7.0f * s);
            g.setColour(juce::Colour(0xffd7a92c));
            g.drawRoundedRectangle(r.toFloat(), 7.0f * s, 1.3f * s);
            g.setColour(juce::Colour(ink));
            g.setFont(uiFont(17.0f * s, juce::Font::bold));
            g.drawFittedText(message,
                             r.reduced(juce::roundToInt(10.0f * s),
                                       juce::roundToInt(7.0f * s)),
                             juce::Justification::centred, 3, 0.93f);
        };

        drawCard(rect(365, 117, 380, 68),
                 "THRESHOLD opens the gate. CLOSE sets its lower closing level; click CLOSE to link.");
        drawCard(rect(43, 573, 361, 76),
                 "LOOKAHEAD sets timing. Drag ATTACK/RELEASE curves vertically for Fast, Linear or Slow. DEPTH goes to -80 dB or infinity.");
        drawCard(rect(515, 516, 184, 90),
                 "MODE selects Gate or Ducking. SIDECHAIN selects the detector source.");
        drawCard(rect(718, 570, 345, 82),
                 "HPF/LPF shape the live detector EQ. INPUT shows the filtered detector; OUTPUT shows gated audio.");
        drawCard(rect(404, 656, 292, 51),
                 "Esc or click empty space to close Help.");
        return;
    }

    // Same modal donation card tokens and QR resource as AutoTrim.
    const auto card = juce::Rectangle<float>(
                          aerosound::ui::overlayCardWidth * s,
                          aerosound::ui::overlayCardHeight * s)
                          .withCentre(getLocalBounds().toFloat().getCentre());

    g.fillAll(juce::Colours::black.withAlpha(aerosound::ui::overlayDimAlpha));
    g.setColour(juce::Colours::black.withAlpha(aerosound::ui::overlayShadowAlpha));
    g.fillRoundedRectangle(card.translated(2.0f * s, 3.0f * s),
                           aerosound::ui::overlayCardRadius * s);
    g.setColour(juce::Colours::white.withAlpha(aerosound::ui::overlayCardFillAlpha));
    g.fillRoundedRectangle(card, aerosound::ui::overlayCardRadius * s);
    g.setColour(juce::Colour(lineBlue).withAlpha(aerosound::ui::overlayCardBorderAlpha));
    g.drawRoundedRectangle(card, aerosound::ui::overlayCardRadius * s, 1.2f * s);

    auto area = card.reduced(18.0f * s);
    g.setColour(juce::Colour(ink));
    g.setFont(uiFont(22.0f * s, juce::Font::bold));
    g.drawText("Support AeroGate", area.removeFromTop(38.0f * s),
               juce::Justification::centred);

    area.removeFromTop(12.0f * s);
    auto qrArea = area.removeFromTop(270.0f * s)
                       .withSizeKeepingCentre(260.0f * s, 260.0f * s);
    if (qrImage.isValid())
    {
        g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
        g.drawImageWithin(qrImage,
                          juce::roundToInt(qrArea.getX()),
                          juce::roundToInt(qrArea.getY()),
                          juce::roundToInt(qrArea.getWidth()),
                          juce::roundToInt(qrArea.getHeight()),
                          juce::RectanglePlacement::centred);
    }

    area.removeFromTop(6.0f * s);
    g.setColour(juce::Colour(mutedInk));
    g.setFont(uiFont(12.5f * s));
    g.drawText("Scan the QR code to support AeroGate development",
               area.toNearestInt(), juce::Justification::centredTop, true);
}

void AeroGateAudioProcessorEditor::PopupOverlay::mouseDown(const juce::MouseEvent&)
{
    if (onDismiss)
        onDismiss();
}

//==============================================================================
AeroGateAudioProcessorEditor::AeroGateAudioProcessorEditor(AeroGateAudioProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      signalFlow(p),
      gatePreview(p),
      detectorScope(p)
{
    setOpaque(true);
    setResizable(true, true);
    setResizeLimits(880, 608, 1320, 912);
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio(1100.0 / 760.0);
    setSize(1100, 760);
    setWantsKeyboardFocus(true);
    setLookAndFeel(&lookAndFeel);

    addAndMakeVisible(signalFlow);
    addAndMakeVisible(gatePreview);
    addAndMakeVisible(detectorScope);

    setupRotary(lookaheadSlider, " ms", 1);
    setupRotary(attackSlider, " ms", 1);
    setupRotary(holdSlider, " ms", 1);
    setupRotary(releaseSlider, " ms", 1);
    setupRotary(hpfSlider, " Hz", 1);
    setupRotary(lpfSlider, " Hz", 1);

    hpfSlider.textFromValueFunction = [](double v)
    {
        if (v >= 1000.0)
            return juce::String(v / 1000.0, 1) + " kHz";
        return juce::String(v, 1) + " Hz";
    };

    lpfSlider.textFromValueFunction = hpfSlider.textFromValueFunction;

    depthSlider.setName("Depth");
    depthSlider.setSliderStyle(juce::Slider::LinearVertical);
    depthSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    depthSlider.setRange(-80.0, 0.0, 0.1);
    depthSlider.setSliderSnapsToMousePosition(false);
    depthSlider.onDragStarted = [this]
    {
        if (processor.getValueTreeState().getRawParameterValue(
                AeroGateAudioProcessor::depthInfParamId)->load() >= 0.5f)
        {
            setParameterValue(AeroGateAudioProcessor::depthInfParamId, 0.0f);
            updateDepthState();
        }
    };
    depthSlider.onClickWithoutDrag = [this]
    {
        const bool inf = processor.getValueTreeState().getRawParameterValue(
            AeroGateAudioProcessor::depthInfParamId)->load() >= 0.5f;
        setParameterValue(AeroGateAudioProcessor::depthInfParamId, inf ? 0.0f : 1.0f);
        updateDepthState();
    };

    std::array<juce::Slider*, 7> sliders {
        &lookaheadSlider, &attackSlider, &holdSlider, &releaseSlider,
        &depthSlider, &hpfSlider, &lpfSlider
    };
    for (auto* slider : sliders)
    {
        slider->onDragStart = [this] { processor.beginHistoryGesture(); };
        slider->onDragEnd = [this] { processor.endHistoryGesture(); };
        addAndMakeVisible(*slider);
    }

    auto& state = processor.getValueTreeState();
    lookaheadAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, AeroGateAudioProcessor::lookaheadParamId, lookaheadSlider);
    attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, AeroGateAudioProcessor::attackParamId, attackSlider);
    holdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, AeroGateAudioProcessor::holdParamId, holdSlider);
    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, AeroGateAudioProcessor::releaseParamId, releaseSlider);
    depthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, AeroGateAudioProcessor::depthParamId, depthSlider);
    hpfAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, AeroGateAudioProcessor::hpfParamId, hpfSlider);
    lpfAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, AeroGateAudioProcessor::lpfParamId, lpfSlider);

    // A cutoff may meet but never cross the other, whether dragged,
    // typed into its value box, or updated by the host while the UI is open.
    hpfSlider.onValueChange = [this]
    {
        if (hpfSlider.getValue() > lpfSlider.getValue())
            hpfSlider.setValue(lpfSlider.getValue(), juce::sendNotificationSync);
    };
    lpfSlider.onValueChange = [this]
    {
        if (lpfSlider.getValue() < hpfSlider.getValue())
            lpfSlider.setValue(hpfSlider.getValue(), juce::sendNotificationSync);
    };

    // Independently recallable, host-automatable HPF and LPF slopes.
    const juce::StringArray slopeLabels {
        "12 dB/oct", "18 dB/oct", "24 dB/oct",
        "36 dB/oct", "48 dB/oct", "96 dB/oct"
    };
    for (auto* box : { &hpfSlopeBox, &lpfSlopeBox })
    {
        for (int i = 0; i < slopeLabels.size(); ++i)
            box->addItem(slopeLabels[i], i + 1);
        box->setColour(juce::ComboBox::backgroundColourId, juce::Colours::white.withAlpha(0.64f));
        box->setColour(juce::ComboBox::outlineColourId,
                       juce::Colour(lineBlue).withAlpha(0.8f));
        box->setColour(juce::ComboBox::textColourId, juce::Colour(ink));
        box->setTooltip("Detector filter steepness");
        addAndMakeVisible(*box);
    }
    hpfSlopeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        state, AeroGateAudioProcessor::hpfSlopeParamId, hpfSlopeBox);
    lpfSlopeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        state, AeroGateAudioProcessor::lpfSlopeParamId, lpfSlopeBox);

    // The editable text is NUMERIC ONLY. Units are separate non-editable labels.
    const auto formatMilliseconds = [](juce::Slider& slider)
    {
        slider.setTextValueSuffix({});
        slider.setNumDecimalPlacesToDisplay(1);
        slider.textFromValueFunction = [](double v) { return juce::String(v, 1); };
        slider.valueFromTextFunction = [](const juce::String& text)
        {
            return text.getDoubleValue();
        };
    };

    for (auto* slider : { &lookaheadSlider, &attackSlider, &holdSlider, &releaseSlider })
        formatMilliseconds(*slider);

    const auto formatFrequency = [](juce::Slider& slider, juce::Label& unit, bool highPass)
    {
        slider.setTextValueSuffix({});
        slider.setNumDecimalPlacesToDisplay(1);
        slider.textFromValueFunction = [highPass](double value)
        {
            if ((highPass && value <= 20.05) || (!highPass && value >= 19999.0))
                return juce::String("Off");
            return value >= 999.95
                ? juce::String(value / 1000.0, 1)
                : juce::String(value, 1);
        };
        slider.valueFromTextFunction = [&unit, highPass](const juce::String& text)
        {
            if (text.trim().equalsIgnoreCase("Off"))
                return highPass ? 20.0 : 20000.0;
            const double scale = unit.getText() == "kHz" ? 1000.0 : 1.0;
            return text.getDoubleValue() * scale;
        };
    };

    formatFrequency(hpfSlider, hpfUnit, true);
    formatFrequency(lpfSlider, lpfUnit, false);

    for (auto* unit : { &lookaheadUnit, &attackUnit, &holdUnit, &releaseUnit,
                         &hpfUnit, &lpfUnit })
    {
        unit->setJustificationType(juce::Justification::centredLeft);
        unit->setColour(juce::Label::textColourId, juce::Colour(ink));
        unit->setFont(uiFont(11.5f));
        unit->setInterceptsMouseClicks(false, false);
        addAndMakeVisible(*unit);
    }

    lookaheadUnit.setText("ms", juce::dontSendNotification);
    attackUnit.setText("ms", juce::dontSendNotification);
    holdUnit.setText("ms", juce::dontSendNotification);
    releaseUnit.setText("ms", juce::dontSendNotification);
    refreshUnitLabels();

    // SliderAttachment has already populated the text boxes using parameter formatting.
    // Immediately rebuild those cached initial strings with our numeric-only formatters,
    // otherwise the previous "5.0 ms" remains visible until the first knob movement.
    for (auto* slider : { &lookaheadSlider, &attackSlider, &holdSlider, &releaseSlider,
                          &hpfSlider, &lpfSlider })
        slider->updateText();

    for (auto* button : { &gateButton, &duckButton, &internalButton, &externalButton,
                          static_cast<juce::TextButton*>(&depthInfButton),
                          &setDefaultButton, &undoButton, &redoButton,
                          &resetButton, &helpButton, &bypassButton, &donateButton })
    {
        setupSmallButton(*button);
        addAndMakeVisible(*button);
    }

    gateButton.onClick = [this] { setParameterValue(AeroGateAudioProcessor::modeParamId, 0.0f); };
    duckButton.onClick = [this] { setParameterValue(AeroGateAudioProcessor::modeParamId, 1.0f); };
    internalButton.onClick = [this] { setParameterValue(AeroGateAudioProcessor::externalSidechainParamId, 0.0f); };
    externalButton.onClick = [this] { setParameterValue(AeroGateAudioProcessor::externalSidechainParamId, 1.0f); };

    depthInfButton.setButtonText(juce::String::fromUTF8("−∞"));
    depthInfButton.onClick = [this]
    {
        const bool inf = processor.getValueTreeState().getRawParameterValue(
            AeroGateAudioProcessor::depthInfParamId)->load() >= 0.5f;
        setParameterValue(AeroGateAudioProcessor::depthInfParamId, inf ? 0.0f : 1.0f);
        updateDepthState();
    };

    addAndMakeVisible(audibleButton);

    audibleButton.onClick = [this]
    {
        setParameterValue(AeroGateAudioProcessor::audibleParamId,
                          audibleButton.getToggleState() ? 1.0f : 0.0f);
        updateAuditionState();
    };


    setDefaultButton.setTooltip("Use current settings as this instance's Reset defaults");
    setDefaultButton.onClick = [this]
    {
        processor.setAsDefault();
        updateHistoryButtons();
    };
    undoButton.setTooltip("Undo last change (right-click to browse full history)");
    redoButton.setTooltip("Redo last undone change");
    undoButton.onHistoryMenu = [this] { showHistoryMenu(); };
    undoButton.onClick = [this]
    {
        if (processor.undo())
        {
            signalFlow.repaint();
            gatePreview.repaint();
            detectorScope.repaint();
            updateHistoryButtons();
        }
    };
    redoButton.onClick = [this]
    {
        if (processor.redo())
        {
            signalFlow.repaint();
            gatePreview.repaint();
            detectorScope.repaint();
            updateHistoryButtons();
        }
    };

    resetButton.onClick = [this] { resetDefaults(); };
    helpButton.onClick = [this]
    {
        helpVisible = !helpOverlay.isVisible();
        donateOverlay.setVisible(false);
        helpOverlay.setBounds(getLocalBounds());
        helpOverlay.setVisible(helpVisible);
        if (helpVisible)
        {
            helpOverlay.toFront(false);
            helpButton.toFront(false);
            grabKeyboardFocus();
        }
    };

    bypassButton.onClick = [this]
    {
        const bool bypassed = processor.getValueTreeState().getRawParameterValue(
            AeroGateAudioProcessor::bypassParamId)->load() >= 0.5f;
        setParameterValue(AeroGateAudioProcessor::bypassParamId, bypassed ? 0.0f : 1.0f);
        syncBypass();
    };

    donateButton.setComponentID("donate-heart");
    donateButton.setTooltip("Support AeroGate development");
    donateButton.onClick = [this]
    {
        helpVisible = false;
        helpOverlay.setVisible(false);
        donateOverlay.setBounds(getLocalBounds());
        donateOverlay.setVisible(true);
        donateOverlay.toFront(false);
    };

    helpOverlay.onDismiss = [this]
    {
        helpVisible = false;
        helpOverlay.setVisible(false);
    };
    donateOverlay.onDismiss = [this]
    {
        donateOverlay.setVisible(false);
    };

    donateOverlay.setQrImage(juce::ImageFileFormat::loadFrom(
        BinaryData::ozon_qr_png, BinaryData::ozon_qr_pngSize));

    addChildComponent(helpOverlay);
    addChildComponent(donateOverlay);
    helpOverlay.setBounds(getLocalBounds());
    donateOverlay.setBounds(getLocalBounds());

    bypassOverlay.setInterceptsMouseClicks(true, true);
    addChildComponent(bypassOverlay);

    updateButtonStates();
    updateDepthState();
    updateAuditionState();
    updateHistoryButtons();

    startTimerHz(30);
}

AeroGateAudioProcessorEditor::~AeroGateAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void AeroGateAudioProcessorEditor::setupRotary(juce::Slider& slider,
                                               const juce::String& suffix,
                                               int decimals)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 22);
    slider.setTextValueSuffix({});
    slider.setNumDecimalPlacesToDisplay(1);
    slider.textFromValueFunction = [](double value) { return juce::String(value, 1); };
    slider.valueFromTextFunction = [](const juce::String& text) { return text.getDoubleValue(); };
    juce::ignoreUnused(suffix, decimals);
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(ink));
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::white.withAlpha(0.34f));
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(lineBlue).withAlpha(0.42f));
    slider.setColour(juce::Slider::textBoxHighlightColourId, juce::Colour(0xff166fb0));
    slider.setColour(juce::TextEditor::highlightColourId, juce::Colour(0xff166fb0));
    slider.setColour(juce::TextEditor::highlightedTextColourId, juce::Colours::white);
    slider.setColour(juce::TextEditor::textColourId, juce::Colour(ink));
    slider.setDoubleClickReturnValue(true, slider.getValue());
    slider.setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void AeroGateAudioProcessorEditor::refreshUnitLabels()
{
    const auto refresh = [](juce::Slider& slider, juce::Label& label, bool highPass)
    {
        const bool off = highPass ? slider.getValue() <= 20.05
                                  : slider.getValue() >= 19999.0;
        const juce::String expected = off ? juce::String()
            : slider.getValue() >= 999.95 ? juce::String("kHz") : juce::String("Hz");
        if (label.getText() != expected)
        {
            label.setText(expected, juce::dontSendNotification);
            slider.updateText();
        }
    };

    refresh(hpfSlider, hpfUnit, true);
    refresh(lpfSlider, lpfUnit, false);
}

void AeroGateAudioProcessorEditor::setupSmallButton(juce::TextButton& button)
{
    button.setColour(juce::TextButton::buttonColourId, juce::Colours::white.withAlpha(0.50f));
    button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(accentStrong));
    button.setColour(juce::TextButton::textColourOffId, juce::Colour(ink));
    button.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    button.setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void AeroGateAudioProcessorEditor::drawBackground(juce::Graphics& g) const
{
    const auto bounds = getLocalBounds().toFloat();

    juce::ColourGradient sky(juce::Colour(aerosound::ui::skyLeft), 0.0f, bounds.getHeight() * 0.10f,
                             juce::Colour(aerosound::ui::skyRight), bounds.getWidth(), bounds.getHeight() * 0.82f,
                             false);
    sky.addColour(0.34, juce::Colour(aerosound::ui::skyMidLeft));
    sky.addColour(0.68, juce::Colour(aerosound::ui::skyMidRight));
    g.setGradientFill(sky);
    g.fillAll();

    juce::ColourGradient haze(juce::Colours::white.withAlpha(0.17f),
                              bounds.getWidth() * 0.28f, bounds.getHeight() * 0.12f,
                              juce::Colours::white.withAlpha(0.0f),
                              bounds.getWidth() * 0.72f, bounds.getHeight() * 0.30f, true);
    g.setGradientFill(haze);
    g.fillEllipse(bounds.getWidth() * -0.04f, bounds.getHeight() * -0.08f,
                  bounds.getWidth() * 0.78f, bounds.getHeight() * 0.42f);

    auto wave = [&](float y, float amp, float phase, juce::Colour colour)
    {
        juce::Path p;
        const float w = bounds.getWidth();
        const float h = bounds.getHeight();
        p.startNewSubPath(0.0f, h * y);
        p.cubicTo(w * 0.23f, h * (y - amp + 0.02f * softWave(phase)),
                  w * 0.47f, h * (y + amp),
                  w * 0.66f, h * (y - amp * 0.28f));
        p.cubicTo(w * 0.82f, h * (y - amp * 0.86f),
                  w * 0.94f, h * (y + amp * 0.62f),
                  w, h * (y - amp * 0.15f));
        p.lineTo(bounds.getBottomRight());
        p.lineTo(bounds.getBottomLeft());
        p.closeSubPath();
        g.setColour(colour);
        g.fillPath(p);
    };

    wave(0.60f, 0.11f, 0.0f, juce::Colour(0xff0878bb).withAlpha(0.08f));
    wave(0.70f, 0.08f, 1.8f, juce::Colour(0xff168dcc).withAlpha(0.07f));
}

void AeroGateAudioProcessorEditor::drawPanel(juce::Graphics& g,
                                             juce::Rectangle<float> panel) const
{
    const float s = aerosound::ui::scaleFor(getWidth(), getHeight());
    g.setColour(juce::Colour(aerosound::ui::panelFill).withAlpha(0.88f));
    g.fillRoundedRectangle(panel, aerosound::ui::panelRadius * s);
    g.setColour(juce::Colours::white.withAlpha(0.64f));
    g.drawRoundedRectangle(panel, aerosound::ui::panelRadius * s, 1.1f * s);
}

void AeroGateAudioProcessorEditor::paint(juce::Graphics& g)
{
    drawBackground(g);

    const float sx = getWidth() / 1100.0f;
    const float sy = getHeight() / 760.0f;
    const float s = juce::jmin(sx, sy);

    auto rect = [sx, sy](float x, float y, float w, float h)
    {
        return juce::Rectangle<float>(x * sx, y * sy, w * sx, h * sy);
    };

    // MODE and SIDECHAIN selectors now belong to their respective signal blocks.
    // The former middle panel's space goes to the envelope and detector graphs.
    drawPanel(g, rect(28, 405, 590, 285));
    drawPanel(g, rect(630, 405, 442, 285));
    drawPanel(g, rect(28, 704, 1044, 42));

    // Reflected halfwave is lightly visible underneath lower glass panels.
    signalFlow.paintWaveReflection(g, signalFlow.getPosition());

    juce::AttributedString title;
    title.setJustification(juce::Justification::centred);
    const auto titleFont = uiFont(39.0f * s, juce::Font::bold);
    title.append("Aero", titleFont, juce::Colour(aerosound::ui::titleInk));
    title.append("Gate", titleFont, juce::Colour(accentStrong));
    title.draw(g, rect(325, 2, 450, 52));

    g.setColour(juce::Colour(mutedInk).withAlpha(0.82f));
    g.setFont(uiFont(12.5f * s));
    g.drawText("by Aerosound", rect(430, 44, 240, 20), juce::Justification::centred);

    g.setColour(juce::Colour(ink));
    g.setFont(uiFont(19.0f * s, juce::Font::bold));
    g.drawText("GATE", rect(45, 418, 75, 28), juce::Justification::centredLeft);
    g.drawText("DETECTOR", rect(648, 418, 106, 28), juce::Justification::centredLeft);

    g.setFont(uiFont(14.0f * s, juce::Font::bold));
    // Per-instance defaults and Undo/Redo replace the old preset bar.

    g.setColour(juce::Colour(lineBlue).withAlpha(0.48f));
    g.drawLine(44.0f * sx, 448.0f * sy, 602.0f * sx, 448.0f * sy, 1.0f * s);
    g.drawLine(646.0f * sx, 448.0f * sy, 1056.0f * sx, 448.0f * sy, 1.0f * s);


}

void AeroGateAudioProcessorEditor::resized()
{
    const float sx = getWidth() / 1100.0f;
    const float sy = getHeight() / 760.0f;
    const float s = juce::jmin(sx, sy);

    auto set = [sx, sy](juce::Component& c, float x, float y, float w, float h)
    {
        c.setBounds(juce::roundToInt(x * sx), juce::roundToInt(y * sy),
                    juce::roundToInt(w * sx), juce::roundToInt(h * sy));
    };

    set(signalFlow, 28, 90, 1044, 300);

    // Full-width GATE chart, with the time controls along its bottom edge.
    set(lookaheadSlider, 52, 591, 102, 85);
    set(attackSlider, 167, 591, 102, 85);
    set(holdSlider, 282, 591, 102, 85);
    set(releaseSlider, 397, 591, 102, 85);
    set(lookaheadUnit, 137, 654, 21, 20);
    set(attackUnit, 252, 654, 21, 20);
    set(holdUnit, 367, 654, 21, 20);
    set(releaseUnit, 482, 654, 21, 20);
    set(depthSlider, 527, 477, 64, 147);
    set(depthInfButton, 529, 626, 60, 31);
    set(gatePreview, 44, 455, 558, 220);

    // Gate mode is chosen beside GATE, not in a separate centre panel.
    set(gateButton, 126, 414, 85, 31);
    set(duckButton, 217, 414, 94, 31);

    // Sidechain source is chosen beside DETECTOR, with Audition alongside.
    set(internalButton, 807, 414, 111, 31);
    set(externalButton, 924, 414, 117, 31);
    set(audibleButton, 759, 415, 29, 29);

    // Expanded detector plot; HPF and LPF remain on opposite edges.
    set(hpfSlider, 652, 568, 95, 80);
    set(lpfSlider, 949, 568, 95, 80);
    set(hpfUnit, 730, 627, 39, 20);
    set(lpfUnit, 1027, 627, 31, 20);
    set(hpfSlopeBox, 650, 654, 108, 26);
    set(lpfSlopeBox, 944, 654, 108, 26);
    set(detectorScope, 646, 455, 410, 220);

    set(setDefaultButton, 48, 711, 151, 28);
    set(undoButton, 207, 711, 68, 28);
    set(redoButton, 283, 711, 68, 28);

    set(resetButton, 720, 711, 78, 28);
    set(helpButton, 806, 711, 78, 28);
    set(bypassButton, 892, 711, 82, 28);
    set(donateButton, 982, 711, 74, 28);

    gatePreview.repaint();
    detectorScope.repaint();

    helpOverlay.setBounds(getLocalBounds());
    donateOverlay.setBounds(getLocalBounds());
    bypassOverlay.setBounds(getLocalBounds());
    if (lastBypass && lastBypassSize != juce::Point<int>(getWidth(), getHeight()))
        refreshBypassSnapshot();
}

void AeroGateAudioProcessorEditor::setParameterValue(const char* id, float value)
{
    if (auto* parameter = processor.getValueTreeState().getParameter(id))
    {
        processor.beginHistoryGesture();
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        parameter->endChangeGesture();
        processor.endHistoryGesture();
    }
}

void AeroGateAudioProcessorEditor::resetDefaults()
{
    processor.resetToDefault();
    updateButtonStates();
    updateDepthState();
    updateAuditionState();
    signalFlow.repaint();
    gatePreview.repaint();
    detectorScope.repaint();
    updateHistoryButtons();
}

void AeroGateAudioProcessorEditor::showHistoryMenu()
{
    processor.captureHistoryStep();
    const auto labels = processor.getHistoryLabels();
    const int current = processor.getHistoryPosition();

    juce::PopupMenu menu;
    menu.addSectionHeader("AeroGate history");
    // Newest first. Entries after the current position are Redo states.
    // JUCE scrolls the popup when there are too many items for the screen.
    for (int index = labels.size() - 1; index >= 0; --index)
    {
        const auto prefix = index == current ? "Current: "
                          : index > current ? "Redo: " : "Undo: ";
        menu.addItem(index + 1, juce::String(prefix) + labels[index],
                     true, index == current);
    }

    juce::Component::SafePointer<AeroGateAudioProcessorEditor> safeEditor(this);
    menu.showMenuAsync(juce::PopupMenu::Options()
                           .withTargetComponent(&undoButton)
                           .withMinimumWidth(330)
                           .withMaximumNumColumns(1),
                       [safeEditor](int choice)
                       {
                           if (safeEditor == nullptr || choice <= 0)
                               return;
                           auto& editor = *safeEditor;
                           if (editor.processor.jumpToHistoryStep(choice - 1))
                           {
                               editor.signalFlow.repaint();
                               editor.gatePreview.repaint();
                               editor.detectorScope.repaint();
                               editor.updateButtonStates();
                               editor.updateDepthState();
                               editor.updateAuditionState();
                               editor.updateHistoryButtons();
                           }
                       });
}

void AeroGateAudioProcessorEditor::updateHistoryButtons()
{
    setDefaultButton.setEnabled(!processor.defaultMatchesCurrent());
    // Keep Undo clickable for its right-click menu even at the oldest state.
    const bool canUndo = processor.canUndo();
    undoButton.setEnabled(true);
    undoButton.setColour(juce::TextButton::buttonColourId,
        canUndo ? juce::Colours::white.withAlpha(0.50f)
                : juce::Colour(0xffd5dce1).withAlpha(0.67f));
    undoButton.setColour(juce::TextButton::textColourOffId,
        canUndo ? juce::Colour(ink) : juce::Colour(0xff7d8993));
    redoButton.setEnabled(processor.canRedo());

    // Grey is deliberately visual only. The second click still works
    // and makes factory settings the new instance-specific default.
    const bool secondStage = processor.isFactoryResetArmed();
    resetButton.setEnabled(true);
    resetButton.setColour(juce::TextButton::buttonColourId,
        secondStage ? juce::Colour(0xffd5dce1).withAlpha(0.67f)
                    : juce::Colours::white.withAlpha(0.50f));
    resetButton.setColour(juce::TextButton::textColourOffId,
        secondStage ? juce::Colour(0xff7d8993) : juce::Colour(ink));
    resetButton.setTooltip(secondStage
        ? "Click again: restore factory settings and make them the instance default"
        : "Reset to this instance's saved default");
}

void AeroGateAudioProcessorEditor::updateButtonStates()
{
    const bool ducking = processor.getValueTreeState().getRawParameterValue(
        AeroGateAudioProcessor::modeParamId)->load() >= 0.5f;
    const bool external = processor.getValueTreeState().getRawParameterValue(
        AeroGateAudioProcessor::externalSidechainParamId)->load() >= 0.5f;

    gateButton.setToggleState(!ducking, juce::dontSendNotification);
    duckButton.setToggleState(ducking, juce::dontSendNotification);
    internalButton.setToggleState(!external, juce::dontSendNotification);
    externalButton.setToggleState(external, juce::dontSendNotification);

    gateButton.setColour(juce::TextButton::buttonColourId,
                         !ducking ? juce::Colour(accentStrong) : juce::Colours::white.withAlpha(0.50f));
    gateButton.setColour(juce::TextButton::textColourOffId,
                         !ducking ? juce::Colours::white : juce::Colour(ink));
    duckButton.setColour(juce::TextButton::buttonColourId,
                         ducking ? juce::Colour(accentStrong) : juce::Colours::white.withAlpha(0.50f));
    duckButton.setColour(juce::TextButton::textColourOffId,
                         ducking ? juce::Colours::white : juce::Colour(ink));

    internalButton.setColour(juce::TextButton::buttonColourId,
                             !external ? juce::Colour(accentStrong) : juce::Colours::white.withAlpha(0.50f));
    internalButton.setColour(juce::TextButton::textColourOffId,
                             !external ? juce::Colours::white : juce::Colour(ink));
    externalButton.setColour(juce::TextButton::buttonColourId,
                             external ? juce::Colour(accentStrong) : juce::Colours::white.withAlpha(0.50f));
    externalButton.setColour(juce::TextButton::textColourOffId,
                             external ? juce::Colours::white : juce::Colour(ink));
}

void AeroGateAudioProcessorEditor::updateDepthState()
{
    const bool inf = processor.getValueTreeState().getRawParameterValue(
        AeroGateAudioProcessor::depthInfParamId)->load() >= 0.5f;
    depthSlider.setInfinity(inf);
    depthSlider.setAlpha(1.0f);
    depthInfButton.setToggleState(inf, juce::dontSendNotification);
    depthInfButton.setColour(juce::TextButton::buttonColourId,
                             inf ? juce::Colour(accentStrong)
                                 : juce::Colours::white.withAlpha(0.50f));
    depthInfButton.setColour(juce::TextButton::textColourOffId,
                             inf ? juce::Colours::white : juce::Colour(ink));
    repaint();
}

void AeroGateAudioProcessorEditor::updateAuditionState()
{
    const bool audible = processor.getValueTreeState().getRawParameterValue(
        AeroGateAudioProcessor::audibleParamId)->load() >= 0.5f;

    audibleButton.setToggleState(audible, juce::dontSendNotification);
}

void AeroGateAudioProcessorEditor::refreshBypassSnapshot()
{
    const bool oldOverlay = bypassOverlay.isVisible();
    const bool oldButton = bypassButton.isVisible();

    bypassOverlay.setVisible(false);
    bypassButton.setVisible(false);

    auto image = createComponentSnapshot(getLocalBounds(), true, 1.0f);
    if (image.isValid())
    {
        for (int y = 0; y < image.getHeight(); ++y)
            for (int x = 0; x < image.getWidth(); ++x)
                image.setPixelAt(x, y, image.getPixelAt(x, y).withSaturation(0.0f));
    }

    bypassOverlay.setSnapshot(std::move(image));
    bypassOverlay.setBounds(getLocalBounds());
    bypassOverlay.setVisible(oldOverlay);
    bypassButton.setVisible(oldButton);
    lastBypassSize = { getWidth(), getHeight() };
}

void AeroGateAudioProcessorEditor::syncBypass()
{
    const bool bypassed = processor.getValueTreeState().getRawParameterValue(
        AeroGateAudioProcessor::bypassParamId)->load() >= 0.5f;

    if (bypassed && (!lastBypass || lastBypassSize != juce::Point<int>(getWidth(), getHeight())))
        refreshBypassSnapshot();

    bypassButton.setToggleState(bypassed, juce::dontSendNotification);
    bypassButton.setColour(juce::TextButton::buttonColourId,
                           bypassed ? juce::Colour(accentStrong)
                                    : juce::Colours::white.withAlpha(0.50f));
    bypassButton.setColour(juce::TextButton::textColourOffId,
                           bypassed ? juce::Colours::white : juce::Colour(ink));

    bypassOverlay.setVisible(bypassed);
    if (bypassed)
    {
        bypassOverlay.toFront(false);
        bypassButton.toFront(false);
    }

    lastBypass = bypassed;
}

void AeroGateAudioProcessorEditor::timerCallback()
{
    AeroGateAudioProcessor::ScopeFrame frame;
    bool gotFrame = false;

    while (processor.popScopeFrame(frame))
    {
        signalFlow.pushFrame(frame);
        gotFrame = true;
    }

    if (gotFrame)
        signalFlow.repaint();

    // Lookahead and envelope automation must update the drawing even while
    // the DAW transport is stopped (when no scope frames are available).
    gatePreview.repaint();
    signalFlow.syncCloseState();

    // Frequency response changes with both cutoffs and slope automation.
    detectorScope.repaint();
    refreshUnitLabels();
    updateButtonStates();
    updateDepthState();
    updateAuditionState();
    syncBypass();
    updateHistoryButtons();
}

bool AeroGateAudioProcessorEditor::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (helpOverlay.isVisible())
        {
            helpVisible = false;
            helpOverlay.setVisible(false);
            return true;
        }

        if (donateOverlay.isVisible())
        {
            donateOverlay.setVisible(false);
            return true;
        }
    }

    return false;
}

void AeroGateAudioProcessorEditor::mouseDown(const juce::MouseEvent& e)
{
    if (helpVisible && e.eventComponent == this)
    {
        helpVisible = false;
        helpOverlay.setVisible(false);
    }
}
