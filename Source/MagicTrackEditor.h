#pragma once

#include "MagicTrackAudioProcessor.h"

#include <JuceHeader.h>
#include <atomic>

namespace magictrack
{
class MagicTrackEditor final : public juce::AudioProcessorEditor,
                              private juce::Timer
{
public:
    explicit MagicTrackEditor(MagicTrackAudioProcessor& processor);
    ~MagicTrackEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void submitCommand();
    void requestSidecarFallback(const juce::String& commandText);
    static juce::String buildSidecarRequestJson(const juce::String& text, const juce::String& profileName);

    void timerCallback() override;

    MagicTrackAudioProcessor& processor;

    juce::TextEditor commandBox;
    juce::TextButton applyButton { "Apply" };
    juce::TextButton undoButton { "Undo Last" };
    juce::TextButton storeAButton { "Store A" };
    juce::TextButton recallAButton { "Recall A" };
    juce::TextButton storeBButton { "Store B" };
    juce::TextButton recallBButton { "Recall B" };
    juce::ToggleButton llmFallbackToggle { "LLM fallback (local sidecar)" };
    juce::TextEditor sidecarUrlBox;
    juce::Label sidecarLabel;
    juce::ComboBox profileBox;
    juce::Label profileLabel;

    juce::Label telemetryLabel;

    std::array<juce::Slider, kMacroCount> sliders;
    std::array<juce::Label, kMacroCount> sliderLabels;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::unique_ptr<ComboBoxAttachment> profileAttachment;
    std::atomic<uint32_t> requestCounter { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MagicTrackEditor)
};
} // namespace magictrack
