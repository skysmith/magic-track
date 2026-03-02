#pragma once

#include "MacroTypes.h"
#include "TextCommandParser.h"

#include <JuceHeader.h>

namespace magictrack
{
class MagicTrackAudioProcessor final : public juce::AudioProcessor
{
public:
    enum class Profile : int
    {
        general = 0,
        guitar,
        vocal,
        uprightBass
    };

    MagicTrackAudioProcessor();
    ~MagicTrackAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

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

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }

    juce::String applyTextCommand(const juce::String& command);
    juce::String applyExternalDeltaJson(const juce::String& jsonResponse, const juce::String& sourceTag);
    void setTelemetryFromUi(const juce::String& message) { telemetryMessage = message; }
    bool wasLastCommandMatchedLocally() const { return lastCommandMatchedLocally; }
    void undoLast();

    void storeSlotA();
    void recallSlotA();
    void storeSlotB();
    void recallSlotB();

    juce::String getTelemetry() const { return telemetryMessage; }
    Profile getProfile() const;

private:
    using Filter = juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>>;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void syncStateFromParameters();
    void pushStateToParameters(const MacroState& state);
    void applyDeltaInternal(const MacroState& delta, const juce::String& telemetryPrefix);
    static juce::String buildDeltaTelemetry(const MacroState& delta, const juce::String& prefix);

    float macro(MacroId id) const;
    int profileIndex() const;
    void updateDspFromMacros();

    juce::AudioProcessorValueTreeState apvts;
    TextCommandParser parser;

    MacroState currentState = kDefaultState;
    MacroState undoState = kDefaultState;
    bool hasUndoState = false;

    MacroState slotA = kDefaultState;
    MacroState slotB = kDefaultState;
    bool slotASet = false;
    bool slotBSet = false;
    bool lastCommandMatchedLocally = false;

    juce::String telemetryMessage = "applied: ready";

    juce::dsp::Gain<float> trim;
    Filter lowShelf;
    Filter highShelf;
    Filter harshPeak;
    juce::dsp::Compressor<float> harshCompressor;
    juce::dsp::Compressor<float> compressor;
    juce::dsp::Gain<float> saturatorDrive;
    juce::dsp::WaveShaper<float> saturator;
    juce::dsp::Reverb reverb;
    juce::AudioBuffer<float> dryBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MagicTrackAudioProcessor)
};
} // namespace magictrack
