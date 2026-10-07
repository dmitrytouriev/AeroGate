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
    const float norm = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f);
    return 150.0f + norm * (30.0f - 150.0f);
}

float SignalFlowComponent::closeAngle(float db) const noexcept
{
    const float norm = juce::jlimit(0.0f, 1.0f, (db + 70.0f) / 70.0f);
    return 210.0f + norm * (330.0f - 210.0f);
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

    juce::Path top;
    juce::Path bottom;
    const float mid = area.getCentreY();
    const float half = area.getHeight() * 0.40f;
    const int count = static_cast<int>(history.size());

    auto sampleAt = [&](int i)
    {
        const int index = newestAtRight ? i : (count - 1 - i);
        return juce::jlimit(0.0f, 1.0f, history[static_cast<size_t>(index)]);
    };

    for (int i = 0; i < count; ++i)
    {
        const float x = area.getX() + area.getWidth() * static_cast<float>(i) / static_cast<float>(count - 1);
        const float amp = std::sqrt(sampleAt(i)) * half;
        if (i == 0)
        {
            top.startNewSubPath(x, mid - amp);
            bottom.startNewSubPath(x, mid + amp);
        }
        else
        {
            top.lineTo(x, mid - amp);
            bottom.lineTo(x, mid + amp);
        }
    }

    juce::Path fill = top;
    for (int i = count - 1; i >= 0; --i)
    {
        const float x = area.getX() + area.getWidth() * static_cast<float>(i) / static_cast<float>(count - 1);
        const float amp = std::sqrt(sampleAt(i)) * half;
        fill.lineTo(x, mid + amp);
    }
    fill.closeSubPath();

    juce::ColourGradient grad(juce::Colour(0xff5bc5f3).withAlpha(0.72f),
                              area.getX(), mid,
                              juce::Colour(0xff168dcc).withAlpha(0.28f),
                              area.getRight(), mid, false);
    g.setGradientFill(grad);
    g.fillPath(fill);

    g.setColour(juce::Colour(accentStrong).withAlpha(0.76f));
    g.strokePath(top, juce::PathStrokeType(0.9f));
    g.strokePath(bottom, juce::PathStrokeType(0.9f));
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
    drawWaveform(g, rightArea, inputHistory, false);

    g.setColour(juce::Colour(ink));
    g.setFont(uiFont(19.0f * s, juce::Font::bold));
    g.drawText("OUTPUT", leftArea.withY(bounds.getY() + 16.0f * s).withHeight(28.0f * s),
               juce::Justification::centred);
    g.drawText("INPUT", rightArea.withY(bounds.getY() + 16.0f * s).withHeight(28.0f * s),
               juce::Justification::centred);

    const float threshold = static_cast<float>(thresholdValue.getValue());
    const float close = juce::jmin(static_cast<float>(closeValue.getValue()), threshold);
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
    g.setColour(juce::Colour(aerosound::ui::meterOrange));
    g.drawText("CLOSE",
               juce::Rectangle<float>(centre.x - innerRadius, centre.y + 73.0f * s,
                                      innerRadius * 2.0f, 20.0f * s),
               juce::Justification::centred);

    g.setFont(uiFont(18.0f * s, juce::Font::bold));
    g.setColour(juce::Colour(0xff27366d));
    g.drawText(juce::String(close, 1) + " dB",
               juce::Rectangle<float>(centre.x - innerRadius, centre.y + 92.0f * s,
                                      innerRadius * 2.0f, 26.0f * s),
               juce::Justification::centred);
}

void SignalFlowComponent::mouseDown(const juce::MouseEvent& e)
{
    const auto centre = getLocalBounds().toFloat().getCentre();
    dragTarget = e.position.x >= centre.x ? DragTarget::threshold : DragTarget::close;
    dragStartY = e.position.y;

    if (dragTarget == DragTarget::threshold)
    {
        dragStartValue = thresholdValue.getValue();
        thresholdCloseGap = juce::jmax(0.0, thresholdValue.getValue() - closeValue.getValue());
    }
    else
    {
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
    constexpr size_t maxHistory = 260;

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
    while (history.size() > 190)
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

    juce::Path top;
    const float mid = chart.getCentreY();
    const float half = chart.getHeight() * 0.42f;

    for (size_t i = 0; i < history.size(); ++i)
    {
        const float x = chart.getX() + chart.getWidth() * static_cast<float>(i)
                                      / static_cast<float>(history.size() - 1);
        const float amp = std::sqrt(history[i]) * half;
        if (i == 0)
            top.startNewSubPath(x, mid - amp);
        else
            top.lineTo(x, mid - amp);
    }

    juce::Path fill = top;
    for (int i = static_cast<int>(history.size()) - 1; i >= 0; --i)
    {
        const float x = chart.getX() + chart.getWidth() * static_cast<float>(i)
                                      / static_cast<float>(history.size() - 1);
        const float amp = std::sqrt(history[static_cast<size_t>(i)]) * half;
        fill.lineTo(x, mid + amp);
    }
    fill.closeSubPath();

    g.setColour(juce::Colour(0xff29aee8).withAlpha(0.56f));
    g.fillPath(fill);
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
    setupRotary(attackSlider, " ms", 2);
    setupRotary(holdSlider, " ms", 0);
    setupRotary(releaseSlider, " ms", 0);
    setupRotary(hpfSlider, " Hz", 0);
    setupRotary(lpfSlider, " Hz", 0);

    hpfSlider.textFromValueFunction = [](double v)
    {
        if (v >= 1000.0)
            return juce::String(v / 1000.0, 2) + " kHz";
        return juce::String(v, 0) + " Hz";
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

    for (auto* button : { &gateButton, &duckButton, &internalButton, &externalButton,
                          &resetButton, &helpButton, &bypassButton, &donateButton,
                          &presetPrev, &presetNext })
    {
        setupSmallButton(*button);
        addAndMakeVisible(*button);
    }

    gateButton.onClick = [this] { setParameterValue(AeroGateAudioProcessor::modeParamId, 0.0f); };
    duckButton.onClick = [this] { setParameterValue(AeroGateAudioProcessor::modeParamId, 1.0f); };
    internalButton.onClick = [this] { setParameterValue(AeroGateAudioProcessor::externalSidechainParamId, 0.0f); };
    externalButton.onClick = [this] { setParameterValue(AeroGateAudioProcessor::externalSidechainParamId, 1.0f); };

    addAndMakeVisible(audibleButton);
    addAndMakeVisible(listenHpfButton);
    addAndMakeVisible(listenLpfButton);

    audibleButton.onClick = [this]
    {
        setParameterValue(AeroGateAudioProcessor::audibleParamId,
                          audibleButton.getToggleState() ? 1.0f : 0.0f);
        updateAuditionState();
    };

    listenHpfButton.onClick = [this]
    {
        const bool on = listenHpfButton.getToggleState();
        setParameterValue(AeroGateAudioProcessor::listenHpfParamId, on ? 1.0f : 0.0f);
        if (on)
            setParameterValue(AeroGateAudioProcessor::listenLpfParamId, 0.0f);
        updateAuditionState();
    };

    listenLpfButton.onClick = [this]
    {
        const bool on = listenLpfButton.getToggleState();
        setParameterValue(AeroGateAudioProcessor::listenLpfParamId, on ? 1.0f : 0.0f);
        if (on)
            setParameterValue(AeroGateAudioProcessor::listenHpfParamId, 0.0f);
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
        helpVisible = !helpVisible;
        repaint();
    };

    bypassButton.onClick = [this]
    {
        const bool bypassed = processor.getValueTreeState().getRawParameterValue(
            AeroGateAudioProcessor::bypassParamId)->load() >= 0.5f;
        setParameterValue(AeroGateAudioProcessor::bypassParamId, bypassed ? 0.0f : 1.0f);
        syncBypass();
    };

    donateButton.setTooltip("Support AeroGate development");

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
    slider.setTextValueSuffix(suffix);
    slider.setNumDecimalPlacesToDisplay(decimals);
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

    g.drawText("HPF", rect(728, 458, 92, 18), juce::Justification::centred);
    g.drawText("LPF", rect(846, 458, 92, 18), juce::Justification::centred);

    const bool depthInf = processor.getValueTreeState().getRawParameterValue(
        AeroGateAudioProcessor::depthInfParamId)->load() >= 0.5f;
    const float depth = processor.getValueTreeState().getRawParameterValue(
        AeroGateAudioProcessor::depthParamId)->load();

    g.setFont(uiFont(14.0f * s, juce::Font::bold));
    g.setColour(juce::Colour(ink));
    g.drawText(depthInf ? juce::String("-inf") : juce::String(depth, 1) + " dB",
               rect(435, 555, 65, 24), juce::Justification::centred);

    g.setFont(uiFont(15.0f * s, juce::Font::bold));
    g.drawText("SIDECHAIN", rect(536, 572, 130, 24), juce::Justification::centredLeft);

    g.setFont(uiFont(14.0f * s, juce::Font::bold));
    g.drawText("PRESET", rect(48, 710, 72, 28), juce::Justification::centredLeft);

    g.setColour(juce::Colour(lineBlue).withAlpha(0.48f));
    g.drawLine(44.0f * sx, 448.0f * sy, 492.0f * sx, 448.0f * sy, 1.0f * s);
    g.drawLine(536.0f * sx, 448.0f * sy, 684.0f * sx, 448.0f * sy, 1.0f * s);
    g.drawLine(728.0f * sx, 448.0f * sy, 1056.0f * sx, 448.0f * sy, 1.0f * s);
    g.drawLine(536.0f * sx, 600.0f * sy, 684.0f * sx, 600.0f * sy, 1.0f * s);

    if (helpVisible)
    {
        g.setColour(juce::Colours::white.withAlpha(0.56f));
        g.fillAll();

        auto card = rect(250, 190, 600, 330);
        g.setColour(juce::Colours::white.withAlpha(0.97f));
        g.fillRoundedRectangle(card, 14.0f * s);
        g.setColour(juce::Colour(lineBlue));
        g.drawRoundedRectangle(card, 14.0f * s, 1.2f * s);

        g.setColour(juce::Colour(ink));
        g.setFont(uiFont(22.0f * s, juce::Font::bold));
        g.drawText("AeroGate", rect(280, 215, 540, 36), juce::Justification::centred);

        g.setFont(uiFont(14.0f * s));
        g.setColour(juce::Colour(mutedInk));
        g.drawFittedText(
            "Drag the blue side of the centre control for Threshold. Drag the orange side for Close. "
            "Threshold moves Close with its current offset. Click Depth without dragging to toggle -inf. "
            "The headphone button auditions the detector; HPF/LPF headphone buttons select a filter stage. "
            "External Sidechain uses the host sidechain bus. Ducking inverts the gate action.",
            rect(300, 270, 500, 180).toNearestInt(),
            juce::Justification::centred, 8);
        g.drawText("Click the background or press Esc to close",
                   rect(300, 465, 500, 26), juce::Justification::centred);
    }
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
    set(depthSlider, 445, 478, 50, 78);
    set(gatePreview, 44, 575, 390, 100);

    set(gateButton, 536, 465, 72, 38);
    set(duckButton, 610, 465, 74, 38);
    set(internalButton, 536, 615, 74, 38);
    set(externalButton, 612, 615, 72, 38);

    set(hpfSlider, 728, 475, 92, 82);
    set(listenHpfButton, 812, 492, 34, 34);
    set(lpfSlider, 846, 475, 92, 82);
    set(listenLpfButton, 930, 492, 34, 34);
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
    setParameterValue(AeroGateAudioProcessor::listenHpfParamId, 0.0f);
    setParameterValue(AeroGateAudioProcessor::listenLpfParamId, 0.0f);
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
    repaint();
}

void AeroGateAudioProcessorEditor::updateAuditionState()
{
    const bool audible = processor.getValueTreeState().getRawParameterValue(
        AeroGateAudioProcessor::audibleParamId)->load() >= 0.5f;
    const bool hpf = processor.getValueTreeState().getRawParameterValue(
        AeroGateAudioProcessor::listenHpfParamId)->load() >= 0.5f;
    const bool lpf = processor.getValueTreeState().getRawParameterValue(
        AeroGateAudioProcessor::listenLpfParamId)->load() >= 0.5f;

    audibleButton.setToggleState(audible, juce::dontSendNotification);
    listenHpfButton.setEnabled(audible);
    listenLpfButton.setEnabled(audible);
    listenHpfButton.setToggleState(audible && hpf, juce::dontSendNotification);
    listenLpfButton.setToggleState(audible && lpf, juce::dontSendNotification);

    if (!audible && (hpf || lpf))
    {
        setParameterValue(AeroGateAudioProcessor::listenHpfParamId, 0.0f);
        setParameterValue(AeroGateAudioProcessor::listenLpfParamId, 0.0f);
    }
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
    if (helpVisible && key == juce::KeyPress::escapeKey)
    {
        helpVisible = false;
        repaint();
        return true;
    }

    return false;
}

void AeroGateAudioProcessorEditor::mouseDown(const juce::MouseEvent& e)
{
    if (helpVisible && e.eventComponent == this)
    {
        helpVisible = false;
        repaint();
    }
}
