#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>

class AeroGateAudioProcessor final : public juce::AudioProcessor
{
public:
    struct ScopeFrame
    {
        float input = 0.0f;
        float output = 0.0f;
        float detector = 0.0f;
    };

    AeroGateAudioProcessor();
    ~AeroGateAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return parameters; }
    const juce::AudioProcessorValueTreeState& getValueTreeState() const noexcept { return parameters; }

    bool popScopeFrame(ScopeFrame&) noexcept;

    static constexpr const char* thresholdParamId = "threshold";
    static constexpr const char* closeParamId = "close";
    static constexpr const char* lookaheadParamId = "lookahead";
    static constexpr const char* attackParamId = "attack";
    static constexpr const char* holdParamId = "hold";
    static constexpr const char* releaseParamId = "release";
    static constexpr const char* depthParamId = "depth";
    static constexpr const char* depthInfParamId = "depthInf";
    static constexpr const char* hpfParamId = "hpf";
    static constexpr const char* lpfParamId = "lpf";
    static constexpr const char* modeParamId = "mode";
    static constexpr const char* externalSidechainParamId = "externalSidechain";
    static constexpr const char* audibleParamId = "audible";
    static constexpr const char* bypassParamId = "bypass";

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void resetDsp();
    void updateFilterCutoffs();
    void updateLatency(float lookaheadMs);
    float processGateEnvelope(float detectorDb, float thresholdDb, float closeDb,
                              float attackMs, float holdMs, float releaseMs) noexcept;
    void pushScopeFrame(float input, float output, float detector) noexcept;

    juce::AudioProcessorValueTreeState parameters;

    juce::dsp::StateVariableTPTFilter<float> bandHp;
    juce::dsp::StateVariableTPTFilter<float> bandLp;

    juce::AudioBuffer<float> delayBuffer;
    int delayWriteIndex = 0;
    int maxDelaySamples = 1;
    int currentLookaheadSamples = 0;

    double currentSampleRate = 48000.0;
    float gateEnvelope = 0.0f;
    bool gateLatched = false;
    int holdRemainingSamples = 0;

    std::array<ScopeFrame, 8192> scopeFrames {};
    juce::AbstractFifo scopeFifo { static_cast<int>(scopeFrames.size()) };
    int scopeDecimation = 32;
    int scopeCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AeroGateAudioProcessor)
};
