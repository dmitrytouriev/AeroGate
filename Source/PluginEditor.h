#pragma once

#include <JuceHeader.h>
#include <deque>
#include <memory>

#include "PluginProcessor.h"
#include "AeroGateControls.h"

class SignalFlowComponent final : public juce::Component
{
public:
    explicit SignalFlowComponent(AeroGateAudioProcessor&);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void pushFrame(const AeroGateAudioProcessor::ScopeFrame&);
    void syncCloseState();
    // Faint reflected half-wave painted behind the lower controls.
    void paintWaveReflection(juce::Graphics&, juce::Point<int> editorOrigin) const;

private:
    enum class DragTarget { none, threshold, close };

    juce::Point<float> arcPoint(float radius, float clockDegrees) const noexcept;
    float thresholdAngle(float db) const noexcept;
    float closeAngle(float db) const noexcept;
    void drawArc(juce::Graphics&, float radius, float fromDeg, float toDeg,
                 juce::Colour, float thickness) const;
    void drawWaveform(juce::Graphics&, juce::Rectangle<float>,
                      const std::deque<float>&, bool newestAtRight) const;
    void setParameterValue(juce::Slider&, double);
    void setCloseEnabled(bool enabled);

    AeroGateAudioProcessor& processor;
    juce::Slider thresholdValue;
    juce::Slider closeValue;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> thresholdAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> closeAttachment;

    std::deque<float> inputHistory;
    std::deque<float> outputHistory;

    DragTarget dragTarget = DragTarget::none;
    float dragStartY = 0.0f;
    double dragStartValue = 0.0;
    double thresholdCloseGap = 6.0;
    double lastUsableCloseGap = 6.0;
};

class GateEnvelopePreview final : public juce::Component
{
public:
    explicit GateEnvelopePreview(AeroGateAudioProcessor&);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    enum class Segment { none, attack, release };
    AeroGateAudioProcessor& processor;
    Segment dragging = Segment::none;
    juce::RangedAudioParameter* dragParameter = nullptr;
    float mouseStartY = 0.0f;
    int currentCurveShape = 1;
    bool gestureStarted = false;
};

class DetectorScope final : public juce::Component
{
public:
    explicit DetectorScope(AeroGateAudioProcessor&);
    void paint(juce::Graphics&) override;

private:
    AeroGateAudioProcessor& processor;
};

class AeroGateAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    explicit AeroGateAudioProcessorEditor(AeroGateAudioProcessor&);
    ~AeroGateAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    class BypassOverlay final : public juce::Component
    {
    public:
        void setSnapshot(juce::Image);
        void paint(juce::Graphics&) override;
    private:
        juce::Image snapshot;
    };

    class PopupOverlay final : public juce::Component
    {
    public:
        explicit PopupOverlay(bool donationPopup);
        void setQrImage(juce::Image);
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        std::function<void()> onDismiss;

    private:
        bool donation = false;
        juce::Image qrImage;
    };

    void timerCallback() override;
    void setupRotary(juce::Slider&, const juce::String& suffix, int decimals);
    void setupSmallButton(juce::TextButton&);
    void refreshUnitLabels();
    void setParameterValue(const char* id, float value);
    void resetDefaults();
    void applyPreset(int index);
    void updateButtonStates();
    void updateDepthState();
    void updateAuditionState();
    void syncBypass();
    void refreshBypassSnapshot();
    void drawPanel(juce::Graphics&, juce::Rectangle<float>) const;
    void drawBackground(juce::Graphics&) const;

    AeroGateAudioProcessor& processor;
    aerosound::ui::AeroLookAndFeel lookAndFeel;

    SignalFlowComponent signalFlow;
    GateEnvelopePreview gatePreview;
    DetectorScope detectorScope;

    juce::Slider lookaheadSlider;
    juce::Slider attackSlider;
    juce::Slider holdSlider;
    juce::Slider releaseSlider;
    aerosound::ui::DepthSlider depthSlider;
    juce::Slider hpfSlider;
    juce::Slider lpfSlider;
    juce::ComboBox hpfSlopeBox;
    juce::ComboBox lpfSlopeBox;
    juce::Label lookaheadUnit;
    juce::Label attackUnit;
    juce::Label holdUnit;
    juce::Label releaseUnit;
    juce::Label hpfUnit;
    juce::Label lpfUnit;

    juce::TextButton gateButton { "Gate" };
    juce::TextButton duckButton { "Ducking" };
    juce::TextButton internalButton { "Internal" };
    juce::TextButton externalButton { "External" };
    aerosound::ui::DepthInfinityButton depthInfButton { "-inf" };

    aerosound::ui::HeadphoneButton audibleButton { "Audible" };

    juce::ComboBox presetBox;
    juce::TextButton presetPrev { "<" };
    juce::TextButton presetNext { ">" };
    juce::TextButton resetButton { "Reset" };
    juce::TextButton helpButton { "Help" };
    juce::TextButton bypassButton { "Bypass" };
    juce::TextButton donateButton { "Donate" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lookaheadAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> holdAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> depthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hpfAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lpfAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> hpfSlopeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lpfSlopeAttachment;

    BypassOverlay bypassOverlay;
    PopupOverlay helpOverlay { false };
    PopupOverlay donateOverlay { true };
    bool lastBypass = false;
    juce::Point<int> lastBypassSize;
    bool helpVisible = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AeroGateAudioProcessorEditor)
};
