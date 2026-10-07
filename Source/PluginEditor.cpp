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

static const char* aeroGateDonateQrBase64 = R"QR(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
RK5CYII=
)QR";
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

    const float w0 = 0.45f + 1.8f * std::sqrt(lookahead / 20.0f);
    const float w1 = 0.65f + 2.2f * std::sqrt(attack / 100.0f);
    const float w2 = 0.65f + 2.2f * std::sqrt(hold / 1000.0f);
    const float w3 = 0.65f + 2.2f * std::sqrt(release / 2000.0f);
    const float sum = w0 + w1 + w2 + w3;

    const float x0 = chart.getX();
    const float x1 = x0 + chart.getWidth() * w0 / sum;
    const float x2 = x1 + chart.getWidth() * w1 / sum;
    const float x3 = x2 + chart.getWidth() * w2 / sum;
    const float x4 = chart.getRight();

    const float topY = chart.getY() + 7.0f;
    const float bottomY = chart.getBottom() - 4.0f;
    const float depthNorm = depthInf ? 0.0f : juce::jlimit(0.0f, 1.0f, (depth + 50.0f) / 50.0f);
    const float depthY = bottomY - depthNorm * (bottomY - topY);
    const float openY = topY;

    const float closedY = depthInf ? bottomY : depthY;
    const float idleY = ducking ? openY : closedY;
    const float activeY = ducking ? closedY : openY;

    g.setColour(juce::Colour(0xff85a8bb).withAlpha(0.14f));
    g.fillRect(juce::Rectangle<float>(x0, chart.getY(), x1 - x0, chart.getHeight()));
    g.setColour(juce::Colour(0xff85a8bb).withAlpha(0.32f));
    for (float x = x0 - chart.getHeight(); x < x1; x += 8.0f)
        g.drawLine(x, chart.getBottom(), x + chart.getHeight(), chart.getY(), 0.8f);

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

    auto label = [&](juce::String text, float a, float b)
    {
        g.drawText(text, juce::Rectangle<float>(a, labelY, b - a, 16.0f),
                   juce::Justification::centred);
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
    setInterceptsMouseClicks(true, true);
    setWantsKeyboardFocus(false);
}

void AeroGateAudioProcessorEditor::PopupOverlay::setQrImage(juce::Image image)
{
    qrImage = std::move(image);
    repaint();
}

void AeroGateAudioProcessorEditor::PopupOverlay::paint(juce::Graphics& g)
{
    const float s = aerosound::ui::scaleFor(getWidth(), getHeight());
    g.fillAll(juce::Colours::black.withAlpha(0.13f));

    const float cardW = (donation ? 380.0f : 650.0f) * s;
    const float cardH = (donation ? 470.0f : 390.0f) * s;
    const auto card = juce::Rectangle<float>(cardW, cardH)
                          .withCentre(getLocalBounds().toFloat().getCentre());

    g.setColour(juce::Colours::black.withAlpha(0.10f));
    g.fillRoundedRectangle(card.translated(2.0f * s, 3.0f * s), 12.0f * s);
    g.setColour(juce::Colours::white.withAlpha(0.98f));
    g.fillRoundedRectangle(card, 12.0f * s);
    g.setColour(juce::Colour(lineBlue).withAlpha(0.90f));
    g.drawRoundedRectangle(card, 12.0f * s, 1.2f * s);

    auto area = card.reduced(22.0f * s);
    g.setColour(juce::Colour(ink));
    g.setFont(uiFont(23.0f * s, juce::Font::bold));

    if (donation)
    {
        g.drawText("Support AeroGate", area.removeFromTop(38.0f * s),
                   juce::Justification::centred);
        area.removeFromTop(10.0f * s);

        auto qrArea = area.removeFromTop(300.0f * s)
                           .withSizeKeepingCentre(285.0f * s, 285.0f * s);
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

        area.removeFromTop(8.0f * s);
        g.setColour(juce::Colour(mutedInk));
        g.setFont(uiFont(12.5f * s));
        g.drawText("Scan the QR code to support AeroGate development",
                   area.toNearestInt(), juce::Justification::centredTop, true);
    }
    else
    {
        g.drawText("AeroGate Help", area.removeFromTop(42.0f * s),
                   juce::Justification::centred);
        area.removeFromTop(8.0f * s);

        g.setColour(juce::Colour(mutedInk));
        g.setFont(uiFont(14.0f * s));
        const juce::String text =
            "THRESHOLD opens the gate. CLOSE is the lower closing threshold and follows Threshold by its stored offset.\n\n"
            "LOOKAHEAD delays the audio path up to 20 ms so the gate can react before the transient reaches the output.\n\n"
            "ATTACK, HOLD and RELEASE shape the gate envelope. DEPTH sets attenuation; the −∞ button fully mutes the closed state.\n\n"
            "HPF and LPF filter only the detector. The headphone button auditions that filtered detector signal.\n\n"
            "SIDECHAIN selects Internal or External detection. DUCKING inverts the gain action.";

        g.drawFittedText(text, area.toNearestInt(),
                         juce::Justification::centredLeft, 14, 0.94f);

        g.setColour(juce::Colour(mutedInk).withAlpha(0.72f));
        g.setFont(uiFont(11.5f * s));
        g.drawText("Click anywhere or press Esc to close",
                   card.withTrimmedTop(card.getHeight() - 30.0f * s).toNearestInt(),
                   juce::Justification::centred);
    }
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

    const auto forceOneDecimalMs = [](juce::Slider& slider)
    {
        slider.setNumDecimalPlacesToDisplay(1);
        slider.textFromValueFunction = [](double value)
        {
            return juce::String(value, 1) + " ms";
        };
    };

    forceOneDecimalMs(lookaheadSlider);
    forceOneDecimalMs(attackSlider);
    forceOneDecimalMs(holdSlider);
    forceOneDecimalMs(releaseSlider);

    hpfSlider.setNumDecimalPlacesToDisplay(1);
    hpfSlider.textFromValueFunction = [](double value)
    {
        return value >= 1000.0
            ? juce::String(value / 1000.0, 1) + " kHz"
            : juce::String(value, 1) + " Hz";
    };

    lpfSlider.setNumDecimalPlacesToDisplay(1);
    lpfSlider.textFromValueFunction = hpfSlider.textFromValueFunction;

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
            helpOverlay.toFront(false);
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

    {
        juce::MemoryOutputStream qrBytes;
        if (juce::Base64::convertFromBase64(qrBytes, aeroGateDonateQrBase64))
            donateOverlay.setQrImage(juce::ImageFileFormat::loadFrom(qrBytes.getData(), qrBytes.getDataSize()));
    }

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
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 76, 22);
    slider.setTextValueSuffix({});
    slider.setNumDecimalPlacesToDisplay(1);
    slider.textFromValueFunction = [suffix](double value)
    {
        return juce::String(value, 1) + suffix;
    };
    juce::ignoreUnused(decimals);
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(ink));
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::white.withAlpha(0.34f));
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(lineBlue).withAlpha(0.42f));
    slider.setColour(juce::Slider::textBoxHighlightColourId, juce::Colour(accentStrong).withAlpha(0.25f));
    slider.setDoubleClickReturnValue(true, slider.getValue());
    slider.setMouseCursor(juce::MouseCursor::PointingHandCursor);
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
    set(depthSlider, 445, 478, 50, 72);
    set(depthInfButton, 438, 576, 60, 28);
    set(gatePreview, 44, 575, 390, 100);

    set(gateButton, 536, 465, 72, 38);
    set(duckButton, 610, 465, 74, 38);
    set(internalButton, 536, 615, 74, 38);
    set(externalButton, 612, 615, 72, 38);

    set(hpfSlider, 742, 475, 104, 82);
    set(lpfSlider, 862, 475, 104, 82);
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

void AeroGateAudioProcessorEditor::mouseDown(const juce::MouseEvent&)
{
}
