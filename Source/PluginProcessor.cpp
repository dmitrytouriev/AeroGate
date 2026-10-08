#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

namespace
{
juce::NormalisableRange<float> makeSkewedRange(float min, float max, float centre)
{
    juce::NormalisableRange<float> r(min, max);
    r.setSkewForCentre(centre);
    return r;
}

float dbFromLinear(float value) noexcept
{
    return juce::Decibels::gainToDecibels(juce::jmax(value, 1.0e-7f), -100.0f);
}

juce::AudioParameterFloatAttributes makeDbAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction([](float v, int)
        {
            return juce::String(v, 1) + " dB";
        });
}

juce::AudioParameterFloatAttributes makeMsAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction([](float v, int)
        {
            return juce::String(v, 1) + " ms";
        });
}

juce::AudioParameterFloatAttributes makeHzAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction([](float v, int)
        {
            return v >= 1000.0f
                ? juce::String(v / 1000.0f, 1) + " kHz"
                : juce::String(v, 1) + " Hz";
        });
}
}

AeroGateAudioProcessor::AeroGateAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                         .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout AeroGateAudioProcessor::createParameterLayout()
{
    using PID = juce::ParameterID;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        PID { thresholdParamId, 1 }, "Threshold",
        juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f), -24.0f, makeDbAttributes()));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        PID { closeParamId, 1 }, "Close",
        juce::NormalisableRange<float>(-70.0f, 0.0f, 0.1f), -30.0f, makeDbAttributes()));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        PID { closeEnabledParamId, 1 }, "Close Enabled", true));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        PID { lookaheadParamId, 1 }, "Lookahead",
        juce::NormalisableRange<float>(0.0f, 20.0f, 0.1f), 5.0f, makeMsAttributes()));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        PID { attackParamId, 1 }, "Attack",
        makeSkewedRange(0.05f, 100.0f, 5.0f), 2.0f, makeMsAttributes()));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        PID { holdParamId, 1 }, "Hold",
        makeSkewedRange(0.0f, 1000.0f, 100.0f), 50.0f, makeMsAttributes()));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        PID { releaseParamId, 1 }, "Release",
        makeSkewedRange(5.0f, 2000.0f, 180.0f), 120.0f, makeMsAttributes()));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        PID { depthParamId, 1 }, "Depth",
        juce::NormalisableRange<float>(-50.0f, 0.0f, 0.1f), -40.0f, makeDbAttributes()));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        PID { depthInfParamId, 1 }, "Depth Infinity", false));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        PID { hpfParamId, 1 }, "Detector HPF",
        makeSkewedRange(20.0f, 2000.0f, 160.0f), 20.0f, makeHzAttributes()));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        PID { lpfParamId, 1 }, "Detector LPF",
        makeSkewedRange(200.0f, 20000.0f, 2000.0f), 1000.0f, makeHzAttributes()));

    const juce::StringArray slopes { "12", "18", "24", "36", "48", "96" };
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        PID { hpfSlopeParamId, 1 }, "HPF Slope", slopes, 2));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        PID { lpfSlopeParamId, 1 }, "LPF Slope", slopes, 2));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        PID { modeParamId, 1 }, "Mode",
        juce::StringArray { "Gate", "Ducking" }, 0));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        PID { externalSidechainParamId, 1 }, "External Sidechain", false));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        PID { audibleParamId, 1 }, "Audible", false));


    layout.add(std::make_unique<juce::AudioParameterBool>(
        PID { bypassParamId, 1 }, "Bypass", false));

    return layout;
}

void AeroGateAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    const juce::dsp::ProcessSpec spec {
        sampleRate,
        static_cast<juce::uint32>(juce::jmax(1, samplesPerBlock)),
        2
    };

    for (auto& filter : bandHp)
    {
        filter.prepare(spec);
        filter.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    }
    for (auto& filter : bandLp)
    {
        filter.prepare(spec);
        filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    }
    cachedHpCutoff = cachedLpCutoff = -1.0f;
    cachedHpSlope = cachedLpSlope = -1;

    maxDelaySamples = juce::jmax(1, juce::roundToInt(sampleRate * 0.020) + samplesPerBlock + 4);
    delayBuffer.setSize(juce::jmax(2, getTotalNumOutputChannels()), maxDelaySamples, false, true, true);
    delayBuffer.clear();

    scopeDecimation = juce::jmax(1, juce::roundToInt(sampleRate / 100.0));
    resetDsp();
    updateFilterCutoffs();
    updateLatency(parameters.getRawParameterValue(lookaheadParamId)->load());
}

void AeroGateAudioProcessor::releaseResources()
{
    delayBuffer.clear();
    resetDsp();
}

void AeroGateAudioProcessor::resetDsp()
{
    for (auto& filter : bandHp)
        filter.reset();
    for (auto& filter : bandLp)
        filter.reset();
    hpOnePoleState.fill(0.0f);
    lpOnePoleState.fill(0.0f);

    delayWriteIndex = 0;
    gateEnvelope = 0.0f;
    gateLatched = false;
    holdRemainingSamples = 0;
    scopeCounter = 0;
    scopeInputPeak = 0.0f;
    scopeOutputPeak = 0.0f;
    scopeDetectorPeak = 0.0f;
}

bool AeroGateAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainIn != mainOut)
        return false;

    if (mainOut != juce::AudioChannelSet::mono()
        && mainOut != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto sidechain = layouts.getChannelSet(true, 1);
        if (!sidechain.isDisabled()
            && sidechain != juce::AudioChannelSet::mono()
            && sidechain != juce::AudioChannelSet::stereo())
            return false;
    }

    return true;
}

void AeroGateAudioProcessor::updateFilterCutoffs()
{
    const float hp = parameters.getRawParameterValue(hpfParamId)->load();
    const float lp = parameters.getRawParameterValue(lpfParamId)->load();

    const float nyquistLimit = static_cast<float>(currentSampleRate * 0.45);
    const float safeHp = juce::jlimit(20.0f, juce::jmin(19000.0f, nyquistLimit),
                                     juce::jmin(hp, lp * 0.95f));
    const float safeLp = juce::jlimit(30.0f, juce::jmin(20000.0f, nyquistLimit),
                                     juce::jmax(lp, safeHp * 1.05f));

    const int hpChoice = juce::jlimit(0, 5, juce::roundToInt(
        parameters.getRawParameterValue(hpfSlopeParamId)->load()));
    const int lpChoice = juce::jlimit(0, 5, juce::roundToInt(
        parameters.getRawParameterValue(lpfSlopeParamId)->load()));

    if (safeHp == cachedHpCutoff && safeLp == cachedLpCutoff
        && hpChoice == cachedHpSlope && lpChoice == cachedLpSlope)
        return;

    static constexpr int orders[] { 2, 3, 4, 6, 8, 16 };
    const auto setStages = [&](auto& filters, int order, float cutoff)
    {
        const int sections = order / 2;
        for (int i = 0; i < sections; ++i)
        {
            // Butterworth poles: each 12 dB/oct section has its own Q.
            // This maintains a ~-3 dB corner when multiple sections are cascaded.
            const float angle = juce::MathConstants<float>::pi *
                                static_cast<float>(2 * i + 1) /
                                static_cast<float>(2 * order);
            const float q = 1.0f / (2.0f * std::cos(angle));
            filters[static_cast<size_t>(i)].setResonance(q);
            filters[static_cast<size_t>(i)].setCutoffFrequency(cutoff);
        }
    };

    const int newHpOrder = orders[hpChoice];
    const int newLpOrder = orders[lpChoice];
    const int newHpCount = newHpOrder / 2;
    const int newLpCount = newLpOrder / 2;

    // On slope changes clear previously inactive stages to avoid stale samples.
    if (hpChoice != cachedHpSlope)
    {
        for (auto& filter : bandHp)
            filter.reset();
        hpOnePoleState.fill(0.0f);
    }
    if (lpChoice != cachedLpSlope)
    {
        for (auto& filter : bandLp)
            filter.reset();
        lpOnePoleState.fill(0.0f);
    }

    setStages(bandHp, newHpOrder, safeHp);
    setStages(bandLp, newLpOrder, safeLp);

    hpStageCount = newHpCount;
    lpStageCount = newLpCount;
    hpExtraPole = (newHpOrder % 2) != 0;
    lpExtraPole = (newLpOrder % 2) != 0;

    const float sr = juce::jmax(1.0f, static_cast<float>(currentSampleRate));
    hpOnePoleAlpha = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * safeHp / sr);
    lpOnePoleAlpha = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * safeLp / sr);

    cachedHpCutoff = safeHp;
    cachedLpCutoff = safeLp;
    cachedHpSlope = hpChoice;
    cachedLpSlope = lpChoice;
}

float AeroGateAudioProcessor::filterHighPass(float sample, int channel) noexcept
{
    for (int i = 0; i < hpStageCount; ++i)
        sample = bandHp[static_cast<size_t>(i)].processSample(channel, sample);

    if (hpExtraPole)
    {
        auto& lowState = hpOnePoleState[static_cast<size_t>(channel)];
        lowState += hpOnePoleAlpha * (sample - lowState);
        sample -= lowState;
    }
    return sample;
}

float AeroGateAudioProcessor::filterLowPass(float sample, int channel) noexcept
{
    for (int i = 0; i < lpStageCount; ++i)
        sample = bandLp[static_cast<size_t>(i)].processSample(channel, sample);

    if (lpExtraPole)
    {
        auto& lowState = lpOnePoleState[static_cast<size_t>(channel)];
        lowState += lpOnePoleAlpha * (sample - lowState);
        sample = lowState;
    }
    return sample;
}

void AeroGateAudioProcessor::updateLatency(float lookaheadMs)
{
    const int newSamples = juce::jlimit(
        0,
        juce::jmax(0, maxDelaySamples - 2),
        juce::roundToInt(currentSampleRate * 0.001 * static_cast<double>(lookaheadMs)));

    if (newSamples != currentLookaheadSamples)
    {
        currentLookaheadSamples = newSamples;
        setLatencySamples(currentLookaheadSamples);
    }
}

float AeroGateAudioProcessor::processGateEnvelope(float detectorDb,
                                                  float thresholdDb,
                                                  float closeDb,
                                                  float attackMs,
                                                  float holdMs,
                                                  float releaseMs) noexcept
{
    closeDb = juce::jmin(closeDb, thresholdDb);

    const int holdSamples = juce::jmax(0, juce::roundToInt(currentSampleRate * 0.001 * holdMs));

    if (detectorDb >= thresholdDb)
    {
        gateLatched = true;
        holdRemainingSamples = holdSamples;
    }
    else if (gateLatched)
    {
        if (detectorDb >= closeDb)
        {
            holdRemainingSamples = holdSamples;
        }
        else if (holdRemainingSamples > 0)
        {
            --holdRemainingSamples;
        }
        else
        {
            gateLatched = false;
        }
    }

    if (gateLatched)
    {
        const float attackSamples = juce::jmax(1.0f, static_cast<float>(currentSampleRate * 0.001 * attackMs));
        gateEnvelope = juce::jmin(1.0f, gateEnvelope + 1.0f / attackSamples);
    }
    else
    {
        const float releaseSamples = juce::jmax(1.0f, static_cast<float>(currentSampleRate * 0.001 * releaseMs));
        gateEnvelope = juce::jmax(0.0f, gateEnvelope - 1.0f / releaseSamples);
    }

    return gateEnvelope;
}

void AeroGateAudioProcessor::pushScopeFrame(float input, float output, float detector) noexcept
{
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    scopeFifo.prepareToWrite(1, start1, size1, start2, size2);
    if (size1 > 0)
    {
        scopeFrames[static_cast<size_t>(start1)] = { input, output, detector };
        scopeFifo.finishedWrite(1);
    }
}

bool AeroGateAudioProcessor::popScopeFrame(ScopeFrame& frame) noexcept
{
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    scopeFifo.prepareToRead(1, start1, size1, start2, size2);
    if (size1 <= 0)
        return false;

    frame = scopeFrames[static_cast<size_t>(start1)];
    scopeFifo.finishedRead(1);
    return true;
}

void AeroGateAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto mainInput = getBusBuffer(buffer, true, 0);
    auto mainOutput = getBusBuffer(buffer, false, 0);

    const int numSamples = mainOutput.getNumSamples();
    const int numOutChannels = mainOutput.getNumChannels();
    const int numInChannels = mainInput.getNumChannels();

    const bool wantsExternal = parameters.getRawParameterValue(externalSidechainParamId)->load() >= 0.5f;
    const bool sidechainAvailable = getBusCount(true) > 1
        && getBus(true, 1) != nullptr
        && getBus(true, 1)->isEnabled();

    auto detectorInput = wantsExternal && sidechainAvailable
        ? getBusBuffer(buffer, true, 1)
        : mainInput;

    const float thresholdDb = parameters.getRawParameterValue(thresholdParamId)->load();
    const bool closeEnabled = parameters.getRawParameterValue(closeEnabledParamId)->load() >= 0.5f;
    const float storedCloseDb = parameters.getRawParameterValue(closeParamId)->load();
    const float closeDb = closeEnabled ? juce::jmin(storedCloseDb, thresholdDb) : thresholdDb;
    const float lookaheadMs = parameters.getRawParameterValue(lookaheadParamId)->load();
    const float attackMs = parameters.getRawParameterValue(attackParamId)->load();
    const float holdMs = parameters.getRawParameterValue(holdParamId)->load();
    const float releaseMs = parameters.getRawParameterValue(releaseParamId)->load();
    const float depthDb = parameters.getRawParameterValue(depthParamId)->load();
    const bool depthInf = parameters.getRawParameterValue(depthInfParamId)->load() >= 0.5f;
    const bool ducking = parameters.getRawParameterValue(modeParamId)->load() >= 0.5f;
    const bool audible = parameters.getRawParameterValue(audibleParamId)->load() >= 0.5f;
    const bool bypassed = parameters.getRawParameterValue(bypassParamId)->load() >= 0.5f;

    updateFilterCutoffs();
    updateLatency(lookaheadMs);

    const float floorGain = depthInf ? 0.0f : juce::Decibels::decibelsToGain(depthDb);

    std::array<float, 2> detectorBand {};
    std::array<float, 2> delayedSamples {};

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float inputScope = 0.0f;
        const int scopeInputChannels = juce::jmax(1, juce::jmin(2, numInChannels));

        for (int ch = 0; ch < juce::jmin(2, numOutChannels); ++ch)
        {
            const float x = ch < numInChannels ? mainInput.getSample(ch, sample) : 0.0f;
            inputScope += x;

            delayBuffer.setSample(ch, delayWriteIndex, x);
            int readIndex = delayWriteIndex - currentLookaheadSamples;
            if (readIndex < 0)
                readIndex += maxDelaySamples;
            delayedSamples[static_cast<size_t>(ch)] = delayBuffer.getSample(ch, readIndex);
        }
        inputScope /= static_cast<float>(scopeInputChannels);

        float detectorPeak = 0.0f;
        float detectorScope = 0.0f;
        const int detectorChannels = juce::jlimit(1, 2, detectorInput.getNumChannels());

        for (int ch = 0; ch < detectorChannels; ++ch)
        {
            const float raw = detectorInput.getSample(ch, sample);
            const float band = filterLowPass(filterHighPass(raw, ch), ch);

            detectorBand[static_cast<size_t>(ch)] = band;
            detectorPeak = juce::jmax(detectorPeak, std::abs(band));
            detectorScope += band;
        }
        detectorScope /= static_cast<float>(detectorChannels);

        const float detectorDb = dbFromLinear(detectorPeak);
        const float env = processGateEnvelope(detectorDb, thresholdDb, closeDb,
                                              attackMs, holdMs, releaseMs);

        const float gateGain = floorGain + env * (1.0f - floorGain);
        const float duckGain = 1.0f - env * (1.0f - floorGain);
        const float appliedGain = ducking ? duckGain : gateGain;

        float outputScope = 0.0f;

        for (int ch = 0; ch < numOutChannels; ++ch)
        {
            float y = delayedSamples[static_cast<size_t>(juce::jmin(ch, 1))];

            if (!bypassed)
            {
                if (audible)
                {
                    const int detectorCh = juce::jmin(ch, detectorChannels - 1);
                    y = detectorBand[static_cast<size_t>(detectorCh)];
                }
                else
                {
                    y *= appliedGain;
                }
            }

            mainOutput.setSample(ch, sample, y);
            outputScope += y;
        }

        outputScope /= static_cast<float>(juce::jmax(1, numOutChannels));

        scopeInputPeak = juce::jmax(scopeInputPeak, std::abs(inputScope));
        scopeOutputPeak = juce::jmax(scopeOutputPeak, std::abs(outputScope));
        scopeDetectorPeak = juce::jmax(scopeDetectorPeak, std::abs(detectorScope));

        if (++scopeCounter >= scopeDecimation)
        {
            scopeCounter = 0;
            pushScopeFrame(juce::jlimit(0.0f, 1.0f, scopeInputPeak),
                           juce::jlimit(0.0f, 1.0f, scopeOutputPeak),
                           juce::jlimit(0.0f, 1.0f, scopeDetectorPeak));
            scopeInputPeak = 0.0f;
            scopeOutputPeak = 0.0f;
            scopeDetectorPeak = 0.0f;
        }

        if (++delayWriteIndex >= maxDelaySamples)
            delayWriteIndex = 0;
    }
}

void AeroGateAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto input = getBusBuffer(buffer, true, 0);
    auto output = getBusBuffer(buffer, false, 0);
    const int numSamples = output.getNumSamples();

    updateLatency(parameters.getRawParameterValue(lookaheadParamId)->load());

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float inputScope = 0.0f;
        float outputScope = 0.0f;

        for (int ch = 0; ch < output.getNumChannels(); ++ch)
        {
            const float x = ch < input.getNumChannels() ? input.getSample(ch, sample) : 0.0f;
            delayBuffer.setSample(ch, delayWriteIndex, x);

            int readIndex = delayWriteIndex - currentLookaheadSamples;
            if (readIndex < 0)
                readIndex += maxDelaySamples;

            const float y = delayBuffer.getSample(ch, readIndex);
            output.setSample(ch, sample, y);

            inputScope += x;
            outputScope += y;
        }

        const float denom = static_cast<float>(juce::jmax(1, output.getNumChannels()));
        inputScope /= denom;
        outputScope /= denom;

        scopeInputPeak = juce::jmax(scopeInputPeak, std::abs(inputScope));
        scopeOutputPeak = juce::jmax(scopeOutputPeak, std::abs(outputScope));
        scopeDetectorPeak = juce::jmax(scopeDetectorPeak, std::abs(inputScope));

        if (++scopeCounter >= scopeDecimation)
        {
            scopeCounter = 0;
            pushScopeFrame(juce::jlimit(0.0f, 1.0f, scopeInputPeak),
                           juce::jlimit(0.0f, 1.0f, scopeOutputPeak),
                           juce::jlimit(0.0f, 1.0f, scopeDetectorPeak));
            scopeInputPeak = 0.0f;
            scopeOutputPeak = 0.0f;
            scopeDetectorPeak = 0.0f;
        }

        if (++delayWriteIndex >= maxDelaySamples)
            delayWriteIndex = 0;
    }
}

juce::AudioProcessorParameter* AeroGateAudioProcessor::getBypassParameter() const
{
    return const_cast<juce::AudioProcessorValueTreeState&>(parameters).getParameter(bypassParamId);
}

void AeroGateAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void AeroGateAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* AeroGateAudioProcessor::createEditor()
{
    return new AeroGateAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AeroGateAudioProcessor();
}
