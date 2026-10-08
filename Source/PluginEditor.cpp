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

    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

juce::Point<float> SignalFlowComponent::arcPoint(float radius, float clockDegrees) const noexcept
{
    const auto centre = getLocalBounds().toFloat().getCentre();
    const float radians = juce::degreesToRadians(clockDegrees);
    return { centre.x + radius * std::sin(radians),
             centre.y - radius * std::cos(radians) };
}

float SignalFlowComponent::thresholdAngle(float db) const noexcept
{
    const auto bounds = getLocalBounds().toFloat();
    const float s = juce::jmax(0.6f, bounds.getHeight() / 300.0f);
    const auto centre = bounds.getCentre();
    const float radius = juce::jmin(bounds.getHeight() * 0.455f, bounds.getWidth() * 0.145f);
    const float waveHeight = bounds.getHeight() - 84.0f * s;
    const float waveMidY = bounds.getY() + 54.0f * s + waveHeight * 0.5f;
    const float waveHalf = waveHeight * 0.43f;

    const float gain = juce::Decibels::decibelsToGain(db);
    const float targetY = waveMidY - std::sqrt(juce::jlimit(0.0f, 1.0f, gain)) * waveHalf;
    const float cosine = juce::jlimit(-1.0f, 1.0f, (centre.y - targetY) / radius);
    return juce::radiansToDegrees(std::acos(cosine));
}

float SignalFlowComponent::closeAngle(float db) const noexcept
{
    const auto bounds = getLocalBounds().toFloat();
    const float s = juce::jmax(0.6f, bounds.getHeight() / 300.0f);
    const auto centre = bounds.getCentre();
    const float radius = juce::jmin(bounds.getHeight() * 0.455f, bounds.getWidth() * 0.145f);
    const float waveHeight = bounds.getHeight() - 84.0f * s;
    const float waveMidY = bounds.getY() + 54.0f * s + waveHeight * 0.5f;
    const float waveHalf = waveHeight * 0.43f;

    const float gain = juce::Decibels::decibelsToGain(db);
    const float targetY = waveMidY - std::sqrt(juce::jlimit(0.0f, 1.0f, gain)) * waveHalf;
    const float cosine = juce::jlimit(-1.0f, 1.0f, (centre.y - targetY) / radius);
    return 360.0f - juce::radiansToDegrees(std::acos(cosine));
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

    const float mid = area.getCentreY();
    const float half = area.getHeight() * 0.43f;
    const int count = static_cast<int>(history.size());

    auto sampleAt = [&](int i)
    {
        const int index = newestAtRight ? i : (count - 1 - i);
        return juce::jlimit(0.0f, 1.0f, history[static_cast<size_t>(index)]);
    };

    g.setColour(juce::Colour(lineBlue).withAlpha(0.28f));
    g.drawHorizontalLine(juce::roundToInt(mid), area.getX(), area.getRight());

    juce::Path top;
    juce::Path fill;
    for (int i = 0; i < count; ++i)
    {
        const float x = area.getX() + area.getWidth() * static_cast<float>(i)
                                      / static_cast<float>(count - 1);
        const float amp = std::sqrt(sampleAt(i)) * half;
        const float y = mid - amp;

        if (i == 0)
            top.startNewSubPath(x, y);
        else
            top.lineTo(x, y);
    }

    fill = top;
    for (int i = count - 1; i >= 0; --i)
    {
        const float x = area.getX() + area.getWidth() * static_cast<float>(i)
                                      / static_cast<float>(count - 1);
        const float amp = std::sqrt(sampleAt(i)) * half;
        fill.lineTo(x, mid + amp);
    }
    fill.closeSubPath();

    juce::ColourGradient grad(juce::Colour(0xff38b8ee).withAlpha(0.54f),
                              area.getX(), mid,
                              juce::Colour(0xff168dcc).withAlpha(0.20f),
                              area.getRight(), mid, false);
    g.setGradientFill(grad);
    g.fillPath(fill);

    g.setColour(juce::Colour(0xff149ee3).withAlpha(0.92f));
    g.strokePath(top, juce::PathStrokeType(1.0f,
                                           juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));
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

    const auto leftArea = juce::Rectangle<float>(
        bounds.getX() + 24.0f * s,
        bounds.getY() + 54.0f * s,
        juce::jmax(20.0f, centre.x - radius * 1.12f - bounds.getX() - 34.0f * s),
        bounds.getHeight() - 84.0f * s);

    const auto rightArea = juce::Rectangle<float>(
        centre.x + radius * 1.12f,
        bounds.getY() + 54.0f * s,
        juce::jmax(20.0f, bounds.getRight() - (centre.x + radius * 1.12f) - 24.0f * s),
        bounds.getHeight() - 84.0f * s);

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

    g.setColour(juce::Colour(aerosound::ui::meterOrange).withAlpha(0.98f));
    juce::Line<float> closeLeft(bounds.getX() + 2.0f * s, closeHandle.y,
                                centre.x - radius * 0.97f, closeHandle.y);
    juce::Line<float> closeRight(centre.x + radius * 0.97f, closeHandle.y,
                                 bounds.getRight() - 2.0f * s, closeHandle.y);
    g.drawDashedLine(closeLeft, dashPattern, 2, 1.5f * s);
    g.drawDashedLine(closeRight, dashPattern, 2, 1.5f * s);

    g.setColour(juce::Colours::white.withAlpha(0.95f));
    g.fillEllipse(centre.x - innerRadius, centre.y - innerRadius,
                  innerRadius * 2.0f, innerRadius * 2.0f);
    g.setColour(juce::Colour(lineBlue).withAlpha(0.56f));
    g.drawEllipse(centre.x - innerRadius, centre.y - innerRadius,
                  innerRadius * 2.0f, innerRadius * 2.0f, 1.2f * s);

    drawArc(g, radius, 150.0f, 30.0f, juce::Colour(0xff91b4c7).withAlpha(0.34f), 13.0f * s);
    drawArc(g, radius, 150.0f, thresholdAngle(threshold), juce::Colour(accentStrong), 13.0f * s);

    drawArc(g, radius, 210.0f, 330.0f, juce::Colour(0xff91b4c7).withAlpha(0.34f), 13.0f * s);
    drawArc(g, radius, 210.0f, closeAngle(close), juce::Colour(aerosound::ui::meterOrange), 13.0f * s);

    auto drawHandle = [&](juce::Point<float> pt, juce::Colour c)
    {
        const float d = 23.0f * s;
        g.setColour(juce::Colours::white.withAlpha(0.98f));
        g.fillEllipse(pt.x - d * 0.5f, pt.y - d * 0.5f, d, d);
        g.setColour(c);
        g.drawEllipse(pt.x - d * 0.5f, pt.y - d * 0.5f, d, d, 2.0f * s);
    };

    drawHandle(thresholdHandle, juce::Colour(accentStrong));
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

    g.setFont(uiFont(13.0f * s, juce::Font::bold));
    g.setColour(closeEnabled
                    ? juce::Colour(aerosound::ui::meterOrange)
                    : juce::Colour(mutedInk).withAlpha(0.52f));
    g.drawText("CLOSE",
               juce::Rectangle<float>(centre.x - innerRadius, centre.y + 73.0f * s,
                                      innerRadius * 2.0f, 20.0f * s),
               juce::Justification::centred);

    g.setFont(uiFont(18.0f * s, juce::Font::bold));
    g.setColour(closeEnabled ? juce::Colour(0xff27366d)
                             : juce::Colour(mutedInk).withAlpha(0.60f));
    g.drawText(juce::String(close, 1) + " dB",
               juce::Rectangle<float>(centre.x - innerRadius, centre.y + 92.0f * s,
                                      innerRadius * 2.0f, 26.0f * s),
               juce::Justification::centred);
}

void SignalFlowComponent::mouseDown(const juce::MouseEvent& e)
{
    const auto bounds = getLocalBounds().toFloat();
    const float s = juce::jmax(0.6f, bounds.getHeight() / 300.0f);
    const auto centre = bounds.getCentre();
    const float radius = juce::jmin(bounds.getHeight() * 0.455f, bounds.getWidth() * 0.145f);
    const float innerRadius = radius * 0.73f;

    const auto closeLabelBounds = juce::Rectangle<float>(
        centre.x - innerRadius,
        centre.y + 70.0f * s,
        innerRadius * 2.0f,
        27.0f * s);

    if (closeLabelBounds.contains(e.position))
    {
        if (auto* parameter = processor.getValueTreeState().getParameter(
                AeroGateAudioProcessor::closeEnabledParamId))
        {
            const bool enabled = processor.getValueTreeState().getRawParameterValue(
                AeroGateAudioProcessor::closeEnabledParamId)->load() >= 0.5f;
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(enabled ? 0.0f : 1.0f);
            parameter->endChangeGesture();
        }

        dragTarget = DragTarget::none;
        repaint();
        return;
    }

    dragTarget = e.position.x >= centre.x ? DragTarget::threshold : DragTarget::close;
    dragStartY = e.position.y;

    if (dragTarget == DragTarget::threshold)
    {
        dragStartValue = thresholdValue.getValue();
        thresholdCloseGap = juce::jmax(0.0, thresholdValue.getValue() - closeValue.getValue());
    }
    else
    {
        const bool enabled = processor.getValueTreeState().getRawParameterValue(
            AeroGateAudioProcessor::closeEnabledParamId)->load() >= 0.5f;

        if (!enabled)
        {
            if (auto* parameter = processor.getValueTreeState().getParameter(
                    AeroGateAudioProcessor::closeEnabledParamId))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost(1.0f);
                parameter->endChangeGesture();
            }
        }

        dragStartValue = closeValue.getValue();
    }
}

void SignalFlowComponent::mouseDrag(const juce::MouseEvent& e)
{
    const double deltaDb = static_cast<double>(dragStartY - e.position.y) * 0.18;

    if (dragTarget == DragTarget::threshold)
    {
        const double newThreshold = juce::jlimit(-60.0, 0.0, dragStartValue + deltaDb);
        const double newClose = juce::jlimit(-70.0, newThreshold, newThreshold - thresholdCloseGap);
        thresholdValue.setValue(newThreshold, juce::sendNotificationSync);
        closeValue.setValue(newClose, juce::sendNotificationSync);
    }
    else if (dragTarget == DragTarget::close)
    {
        const double threshold = thresholdValue.getValue();
        closeValue.setValue(juce::jlimit(-70.0, threshold, dragStartValue + deltaDb),
                            juce::sendNotificationSync);
    }

    repaint();
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
    setInterceptsMouseClicks(false, false);
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

    // All four sections use one time scale: equal milliseconds = equal widths.
    const float totalMs = juce::jmax(0.001f, lookahead + attack + hold + release);
    const float pxPerMs = chart.getWidth() / totalMs;
    const float x0 = chart.getX();
    const float x1 = x0 + lookahead * pxPerMs;
    const float x2 = x1 + attack * pxPerMs;
    const float x3 = x2 + hold * pxPerMs;
    const float x4 = chart.getRight();

    const float topY = chart.getY() + 7.0f;
    const float bottomY = chart.getBottom() - 4.0f;
    const float depthNorm = depthInf ? 0.0f : juce::jlimit(0.0f, 1.0f, (depth + 50.0f) / 50.0f);
    const float depthY = bottomY - depthNorm * (bottomY - topY);
    const float openY = topY;

    const float closedY = depthInf ? bottomY : depthY;
    const float idleY = ducking ? openY : closedY;
    const float activeY = ducking ? closedY : openY;

    // Lookahead is a separate timeline region; never let its hatching bleed into Attack/Hold.
    const juce::Rectangle<float> lookaheadArea(x0, chart.getY(),
                                               juce::jmax(0.0f, x1 - x0), chart.getHeight());
    if (lookaheadArea.getWidth() > 0.0f)
    {
        g.saveState();
        g.reduceClipRegion(lookaheadArea.getSmallestIntegerContainer());
        g.setColour(juce::Colour(0xff85a8bb).withAlpha(0.14f));
        g.fillRect(lookaheadArea);
        g.setColour(juce::Colour(0xff85a8bb).withAlpha(0.32f));
        for (float x = x0 - chart.getHeight(); x < x1; x += 8.0f)
            g.drawLine(x, chart.getBottom(), x + chart.getHeight(), chart.getY(), 0.8f);
        g.restoreState();
    }

    g.setColour(juce::Colour(lineBlue).withAlpha(0.48f));
    for (float x : { x1, x2, x3, x4 })
        g.drawVerticalLine(juce::roundToInt(x), chart.getY(), chart.getBottom());

    juce::Path env;
    env.startNewSubPath(x0, idleY);
    env.lineTo(x1, idleY);
    env.cubicTo(x1 + (x2 - x1) * 0.28f, idleY,
                x1 + (x2 - x1) * 0.72f, activeY,
                x2, activeY);
    env.lineTo(x3, activeY);
    env.cubicTo(x3 + (x4 - x3) * 0.28f, activeY,
                x3 + (x4 - x3) * 0.72f, idleY,
                x4, idleY);

    juce::Path fill = env;
    fill.lineTo(x4, bottomY);
    fill.lineTo(x0, bottomY);
    fill.closeSubPath();

    g.setColour(juce::Colour(0xff64c4ed).withAlpha(0.22f));
    g.fillPath(fill);
    g.setColour(juce::Colour(accentStrong));
    g.strokePath(env, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));

    const auto labelY = r.getY() + 3.0f;
    g.setFont(uiFont(9.5f, juce::Font::bold));
    g.setColour(juce::Colour(mutedInk));

    auto label = [&](const juce::String& text, float a, float b)
    {
        const float width = b - a;
        // A short interval should not paint clipped text (e.g. "L..." across other sections).
        if (width >= text.length() * 5.3f)
            g.drawText(text, juce::Rectangle<float>(a, labelY, width, 16.0f),
                       juce::Justification::centred, false);
    };

    label("LOOKAHEAD", x0, x1);
    label("ATTACK", x1, x2);
    label("HOLD", x2, x3);
    label("RELEASE", x3, x4);
}

//==============================================================================
void DetectorScope::push(float value)
{
    history.push_back(juce::jlimit(0.0f, 1.0f, value));
    while (history.size() > 400)
        history.pop_front();
    repaint();
}

void DetectorScope::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(juce::Colours::white.withAlpha(0.30f));
    g.fillRoundedRectangle(r, 8.0f);
    g.setColour(juce::Colour(lineBlue).withAlpha(0.50f));
    g.drawRoundedRectangle(r, 8.0f, 1.0f);

    g.setColour(juce::Colour(mutedInk));
    g.setFont(uiFont(10.5f));
    g.drawText("Detector Signal", r.reduced(10.0f, 5.0f).removeFromTop(17.0f),
               juce::Justification::centredLeft);

    auto chart = r.reduced(10.0f, 8.0f);
    chart.removeFromTop(20.0f);

    if (history.size() < 2)
        return;

    const float mid = chart.getCentreY();
    const float half = chart.getHeight() * 0.42f;

    g.setColour(juce::Colour(lineBlue).withAlpha(0.28f));
    g.drawHorizontalLine(juce::roundToInt(mid), chart.getX(), chart.getRight());

    juce::Path top;
    juce::Path fill;

    for (size_t i = 0; i < history.size(); ++i)
    {
        const float x = chart.getX() + chart.getWidth() * static_cast<float>(i)
                                      / static_cast<float>(history.size() - 1);
        const float amp = std::sqrt(history[i]) * half;
        const float y = mid - amp;

        if (i == 0)
            top.startNewSubPath(x, y);
        else
            top.lineTo(x, y);
    }

    fill = top;
    for (int i = static_cast<int>(history.size()) - 1; i >= 0; --i)
    {
        const float x = chart.getX() + chart.getWidth() * static_cast<float>(i)
                                      / static_cast<float>(history.size() - 1);
        const float amp = std::sqrt(history[static_cast<size_t>(i)]) * half;
        fill.lineTo(x, mid + amp);
    }
    fill.closeSubPath();

    g.setColour(juce::Colour(0xff29aee8).withAlpha(0.35f));
    g.fillPath(fill);
    g.setColour(juce::Colour(0xff149ee3).withAlpha(0.90f));
    g.strokePath(top, juce::PathStrokeType(1.0f,
                                           juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));
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
                 "LOOKAHEAD, ATTACK, HOLD and RELEASE shape the envelope. DEPTH controls attenuation.");
        drawCard(rect(515, 516, 184, 90),
                 "MODE selects Gate or Ducking. SIDECHAIN selects the detector source.");
        drawCard(rect(718, 570, 345, 82),
                 "HPF and LPF filter the detector. Headphones let you listen to it.");
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
      gatePreview(p)
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

    depthSlider.setSliderStyle(juce::Slider::LinearVertical);
    depthSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    depthSlider.setRange(-50.0, 0.0, 0.1);
    depthSlider.setSliderSnapsToMousePosition(false);
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
        addAndMakeVisible(*slider);

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

    const auto formatFrequency = [](juce::Slider& slider, juce::Label& unit)
    {
        slider.setTextValueSuffix({});
        slider.setNumDecimalPlacesToDisplay(1);
        slider.textFromValueFunction = [](double value)
        {
            return value >= 999.95
                ? juce::String(value / 1000.0, 1)
                : juce::String(value, 1);
        };
        slider.valueFromTextFunction = [&unit](const juce::String& text)
        {
            const double scale = unit.getText() == "kHz" ? 1000.0 : 1.0;
            return text.getDoubleValue() * scale;
        };
    };

    formatFrequency(hpfSlider, hpfUnit);
    formatFrequency(lpfSlider, lpfUnit);

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
                          &depthInfButton, &resetButton, &helpButton, &bypassButton, &donateButton,
                          &presetPrev, &presetNext })
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


    presetBox.addItem("Default", 1);
    presetBox.addItem("Kick Tight", 2);
    presetBox.addItem("Tom Natural", 3);
    presetBox.addItem("Ducking", 4);
    presetBox.setSelectedId(2, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        applyPreset(presetBox.getSelectedItemIndex());
    };
    addAndMakeVisible(presetBox);

    presetPrev.onClick = [this]
    {
        const int count = presetBox.getNumItems();
        if (count <= 0) return;
        const int next = (presetBox.getSelectedItemIndex() - 1 + count) % count;
        presetBox.setSelectedItemIndex(next, juce::sendNotification);
    };

    presetNext.onClick = [this]
    {
        const int count = presetBox.getNumItems();
        if (count <= 0) return;
        const int next = (presetBox.getSelectedItemIndex() + 1) % count;
        presetBox.setSelectedItemIndex(next, juce::sendNotification);
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
    const auto refresh = [](juce::Slider& slider, juce::Label& label)
    {
        const juce::String expected = slider.getValue() >= 999.95 ? "kHz" : "Hz";
        if (label.getText() != expected)
        {
            label.setText(expected, juce::dontSendNotification);
            slider.updateText();
        }
    };

    refresh(hpfSlider, hpfUnit);
    refresh(lpfSlider, lpfUnit);
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

    drawPanel(g, rect(28, 405, 480, 285));
    drawPanel(g, rect(520, 405, 180, 285));
    drawPanel(g, rect(712, 405, 360, 285));
    drawPanel(g, rect(28, 704, 1044, 42));

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
    g.drawText("GATE", rect(45, 418, 100, 28), juce::Justification::centredLeft);
    g.drawText("MODE", rect(536, 418, 100, 28), juce::Justification::centredLeft);
    g.drawText("DETECTOR", rect(730, 418, 150, 28), juce::Justification::centredLeft);

    g.setFont(uiFont(11.5f * s));
    g.setColour(juce::Colour(mutedInk));
    g.drawText("LOOKAHEAD", rect(45, 458, 92, 18), juce::Justification::centred);
    g.drawText("ATTACK", rect(143, 458, 92, 18), juce::Justification::centred);
    g.drawText("HOLD", rect(241, 458, 92, 18), juce::Justification::centred);
    g.drawText("RELEASE", rect(339, 458, 92, 18), juce::Justification::centred);
    g.drawText("DEPTH", rect(438, 458, 58, 18), juce::Justification::centred);

    g.drawText("HPF", rect(742, 458, 104, 18), juce::Justification::centred);
    g.drawText("LPF", rect(862, 458, 104, 18), juce::Justification::centred);

    const bool depthInf = processor.getValueTreeState().getRawParameterValue(
        AeroGateAudioProcessor::depthInfParamId)->load() >= 0.5f;
    const float depth = processor.getValueTreeState().getRawParameterValue(
        AeroGateAudioProcessor::depthParamId)->load();

    g.setFont(uiFont(14.0f * s, juce::Font::bold));
    g.setColour(juce::Colour(ink));
    g.drawText(depthInf ? juce::String::fromUTF8("−∞") : juce::String(depth, 1) + " dB",
               rect(435, 550, 65, 22), juce::Justification::centred);

    g.setFont(uiFont(15.0f * s, juce::Font::bold));
    g.drawText("SIDECHAIN", rect(536, 572, 130, 24), juce::Justification::centredLeft);

    g.setFont(uiFont(14.0f * s, juce::Font::bold));
    g.drawText("PRESET", rect(48, 710, 72, 28), juce::Justification::centredLeft);

    g.setColour(juce::Colour(lineBlue).withAlpha(0.48f));
    g.drawLine(44.0f * sx, 448.0f * sy, 492.0f * sx, 448.0f * sy, 1.0f * s);
    g.drawLine(536.0f * sx, 448.0f * sy, 684.0f * sx, 448.0f * sy, 1.0f * s);
    g.drawLine(728.0f * sx, 448.0f * sy, 1056.0f * sx, 448.0f * sy, 1.0f * s);
    g.drawLine(536.0f * sx, 600.0f * sy, 684.0f * sx, 600.0f * sy, 1.0f * s);


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

    set(lookaheadSlider, 45, 475, 92, 82);
    set(attackSlider, 143, 475, 92, 82);
    set(holdSlider, 241, 475, 92, 82);
    set(releaseSlider, 339, 475, 92, 82);
    set(lookaheadUnit, 121, 534, 21, 20);
    set(attackUnit, 219, 534, 21, 20);
    set(holdUnit, 317, 534, 21, 20);
    set(releaseUnit, 415, 534, 21, 20);
    set(depthSlider, 445, 478, 50, 72);
    set(depthInfButton, 438, 576, 60, 28);
    set(gatePreview, 44, 575, 390, 100);

    set(gateButton, 536, 465, 72, 38);
    set(duckButton, 610, 465, 74, 38);
    set(internalButton, 536, 615, 74, 38);
    set(externalButton, 612, 615, 72, 38);

    set(hpfSlider, 742, 475, 104, 82);
    set(lpfSlider, 862, 475, 104, 82);
    set(hpfUnit, 825, 534, 38, 20);
    set(lpfUnit, 945, 534, 38, 20);
    set(audibleButton, 986, 482, 58, 58);
    set(detectorScope, 728, 575, 328, 100);

    set(presetBox, 122, 711, 200, 28);
    set(presetPrev, 330, 711, 42, 28);
    set(presetNext, 378, 711, 42, 28);

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
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        parameter->endChangeGesture();
    }
}

void AeroGateAudioProcessorEditor::resetDefaults()
{
    setParameterValue(AeroGateAudioProcessor::thresholdParamId, -24.0f);
    setParameterValue(AeroGateAudioProcessor::closeParamId, -30.0f);
    setParameterValue(AeroGateAudioProcessor::closeEnabledParamId, 1.0f);
    setParameterValue(AeroGateAudioProcessor::lookaheadParamId, 5.0f);
    setParameterValue(AeroGateAudioProcessor::attackParamId, 2.0f);
    setParameterValue(AeroGateAudioProcessor::holdParamId, 50.0f);
    setParameterValue(AeroGateAudioProcessor::releaseParamId, 120.0f);
    setParameterValue(AeroGateAudioProcessor::depthParamId, -40.0f);
    setParameterValue(AeroGateAudioProcessor::depthInfParamId, 0.0f);
    setParameterValue(AeroGateAudioProcessor::hpfParamId, 20.0f);
    setParameterValue(AeroGateAudioProcessor::lpfParamId, 1000.0f);
    setParameterValue(AeroGateAudioProcessor::modeParamId, 0.0f);
    setParameterValue(AeroGateAudioProcessor::externalSidechainParamId, 0.0f);
    setParameterValue(AeroGateAudioProcessor::audibleParamId, 0.0f);
    updateButtonStates();
    updateDepthState();
    updateAuditionState();
}

void AeroGateAudioProcessorEditor::applyPreset(int index)
{
    resetDefaults();

    if (index == 1) // Kick Tight
    {
        setParameterValue(AeroGateAudioProcessor::attackParamId, 1.0f);
        setParameterValue(AeroGateAudioProcessor::holdParamId, 55.0f);
        setParameterValue(AeroGateAudioProcessor::releaseParamId, 110.0f);
    }
    else if (index == 2) // Tom Natural
    {
        setParameterValue(AeroGateAudioProcessor::thresholdParamId, -30.0f);
        setParameterValue(AeroGateAudioProcessor::closeParamId, -36.0f);
        setParameterValue(AeroGateAudioProcessor::attackParamId, 2.5f);
        setParameterValue(AeroGateAudioProcessor::holdParamId, 120.0f);
        setParameterValue(AeroGateAudioProcessor::releaseParamId, 320.0f);
        setParameterValue(AeroGateAudioProcessor::depthParamId, -30.0f);
        setParameterValue(AeroGateAudioProcessor::hpfParamId, 35.0f);
        setParameterValue(AeroGateAudioProcessor::lpfParamId, 1800.0f);
    }
    else if (index == 3) // Ducking
    {
        setParameterValue(AeroGateAudioProcessor::modeParamId, 1.0f);
        setParameterValue(AeroGateAudioProcessor::depthParamId, -18.0f);
        setParameterValue(AeroGateAudioProcessor::releaseParamId, 250.0f);
    }

    updateButtonStates();
    updateDepthState();
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
    depthSlider.setAlpha(inf ? 0.42f : 1.0f);
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
        detectorScope.push(frame.detector);
        gotFrame = true;
    }

    if (gotFrame)
    {
        gatePreview.repaint();
        signalFlow.repaint();
    }

    refreshUnitLabels();
    updateButtonStates();
    updateDepthState();
    updateAuditionState();
    syncBypass();
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
