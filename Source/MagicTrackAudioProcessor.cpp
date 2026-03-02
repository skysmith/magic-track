#include "MagicTrackAudioProcessor.h"
#include "MagicTrackEditor.h"
#include "BinaryData.h"

#include <cmath>

namespace magictrack
{
namespace
{
constexpr float kDeltaEpsilon = 0.01f;

struct ProfileTuning
{
    float levelMinDb;
    float levelMaxDb;
    float lowShelfFrequency;
    float lowShelfMinDb;
    float lowShelfMaxDb;
    float highShelfFrequency;
    float highShelfMinDb;
    float highShelfMaxDb;
    float harshFrequency;
    float harshQ;
    float harshEqAtMinMacroDb;
    float harshEqAtMaxMacroDb;
    float harshCompThresholdMin;
    float harshCompThresholdMax;
    float harshCompRatioMin;
    float harshCompRatioMax;
    float compThresholdMin;
    float compThresholdMax;
    float compRatioMin;
    float compRatioMax;
    float compAttackMinMs;
    float compAttackMaxMs;
    float compReleaseMinMs;
    float compReleaseMaxMs;
    float saturatorDriveMinDb;
    float saturatorDriveMaxDb;
    float widthMin;
    float widthMax;
    float reverbRoomMin;
    float reverbRoomMax;
    float wetMixMin;
    float wetMixMax;
    float reverbDamping;
};

inline ProfileTuning tuningForProfile(int index)
{
    switch (index)
    {
        case 1: // Guitar
            return {
                -18.0f, 18.0f,
                170.0f, -8.0f, 6.0f,
                4200.0f, -8.0f, 9.0f,
                3600.0f, 1.5f, 5.0f, -12.0f,
                -26.0f, -8.0f, 6.5f, 1.6f,
                -30.0f, -10.0f, 1.8f, 4.6f,
                1.5f, 28.0f, 55.0f, 180.0f,
                0.0f, 22.0f,
                0.0f, 2.1f,
                0.05f, 0.82f,
                0.0f, 0.35f,
                0.52f
            };
        case 2: // Vocal
            return {
                -18.0f, 12.0f,
                220.0f, -7.0f, 5.0f,
                7800.0f, -6.0f, 7.5f,
                5400.0f, 1.2f, 3.0f, -11.0f,
                -26.0f, -8.0f, 6.0f, 1.7f,
                -26.0f, -12.0f, 1.8f, 3.8f,
                3.0f, 18.0f, 70.0f, 210.0f,
                0.0f, 8.0f,
                0.7f, 1.6f,
                0.1f, 0.9f,
                0.0f, 0.45f,
                0.62f
            };
        case 3: // Upright Bass
            return {
                -16.0f, 14.0f,
                120.0f, -10.0f, 8.0f,
                3000.0f, -6.5f, 5.0f,
                1800.0f, 1.0f, 2.5f, -8.5f,
                -21.0f, -6.0f, 4.5f, 1.5f,
                -28.0f, -11.0f, 1.7f, 4.0f,
                5.0f, 35.0f, 90.0f, 260.0f,
                0.0f, 10.0f,
                0.8f, 1.3f,
                0.06f, 0.7f,
                0.0f, 0.32f,
                0.66f
            };
        default: // General
            return {
                -18.0f, 18.0f,
                200.0f, -9.0f, 9.0f,
                5000.0f, -9.0f, 9.0f,
                3400.0f, 1.4f, 6.0f, -12.0f,
                -24.0f, -6.0f, 6.0f, 1.5f,
                -28.0f, -10.0f, 1.6f, 4.0f,
                2.0f, 30.0f, 60.0f, 180.0f,
                0.0f, 18.0f,
                0.0f, 2.0f,
                0.1f, 0.95f,
                0.0f, 0.45f,
                0.55f
            };
    }
}

inline float normalizedToRange(float macroValue, float minVal, float maxVal)
{
    auto t = juce::jlimit(0.0f, 1.0f, macroValue / 100.0f);
    return juce::jmap(t, minVal, maxVal);
}

inline juce::String macroParamId(MacroId id)
{
    return juce::String(kMacroNames[static_cast<size_t>(id)]);
}
}

MagicTrackAudioProcessor::MagicTrackAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "MagicTrackParameters", createParameterLayout())
{
    saturator.functionToUse = [](float x) { return std::tanh(x); };

    auto phraseData = juce::String::fromUTF8(BinaryData::phrases_v1_json, BinaryData::phrases_v1_jsonSize);
    if (!parser.loadFromJsonString(phraseData))
        telemetryMessage = "applied: phrase dictionary failed to load";

    syncStateFromParameters();
}

void MagicTrackAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    trim.prepare(spec);
    lowShelf.prepare(spec);
    highShelf.prepare(spec);
    harshPeak.prepare(spec);
    harshCompressor.prepare(spec);
    compressor.prepare(spec);
    saturatorDrive.prepare(spec);
    saturator.prepare(spec);
    reverb.prepare(spec);
    dryBuffer.setSize(getTotalNumOutputChannels(), samplesPerBlock, false, false, true);

    updateDspFromMacros();
}

void MagicTrackAudioProcessor::releaseResources()
{
}

bool MagicTrackAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    if (input != output)
        return false;

    return input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo();
}

void MagicTrackAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    syncStateFromParameters();
    updateDspFromMacros();
    const auto tuning = tuningForProfile(profileIndex());

    if (dryBuffer.getNumChannels() != buffer.getNumChannels() || dryBuffer.getNumSamples() < buffer.getNumSamples())
        dryBuffer.setSize(buffer.getNumChannels(), buffer.getNumSamples(), false, false, true);

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        dryBuffer.copyFrom(channel, 0, buffer, channel, 0, buffer.getNumSamples());

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);

    trim.process(context);
    lowShelf.process(context);
    highShelf.process(context);
    harshPeak.process(context);
    harshCompressor.process(context);
    compressor.process(context);
    saturatorDrive.process(context);
    saturator.process(context);

    if (buffer.getNumChannels() >= 2)
    {
        auto widthValue = macro(MacroId::width);
        auto widthScale = normalizedToRange(widthValue, tuning.widthMin, tuning.widthMax);
        auto* left = buffer.getWritePointer(0);
        auto* right = buffer.getWritePointer(1);

        constexpr float kInvSqrt2 = 0.70710678f;
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            auto mid = (left[sample] + right[sample]) * kInvSqrt2;
            auto side = (left[sample] - right[sample]) * kInvSqrt2 * widthScale;
            left[sample] = (mid + side) * kInvSqrt2;
            right[sample] = (mid - side) * kInvSqrt2;
        }
    }

    juce::dsp::AudioBlock<float> wetBlock(buffer);
    juce::dsp::ProcessContextReplacing<float> wetContext(wetBlock);
    reverb.process(wetContext);

    auto wetMix = normalizedToRange(macro(MacroId::space), tuning.wetMixMin, tuning.wetMixMax);
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* wet = buffer.getWritePointer(channel);
        auto* dryRead = dryBuffer.getReadPointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            wet[sample] = (dryRead[sample] * (1.0f - wetMix)) + (wet[sample] * wetMix);
    }
}

juce::AudioProcessorEditor* MagicTrackAudioProcessor::createEditor()
{
    return new MagicTrackEditor(*this);
}

void MagicTrackAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto tree = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(tree.createXml());
    copyXmlToBinary(*xml, destData);
}

void MagicTrackAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
        syncStateFromParameters();
    }
}

juce::String MagicTrackAudioProcessor::applyTextCommand(const juce::String& command)
{
    auto parsed = parser.parse(command);
    lastCommandMatchedLocally = parsed.matched;

    if (!parsed.matched)
    {
        telemetryMessage = parsed.telemetry;
        return telemetryMessage;
    }

    if (parsed.resetToDefaults)
    {
        undoState = currentState;
        hasUndoState = true;
        currentState = kDefaultState;
        pushStateToParameters(currentState);
        telemetryMessage = parsed.telemetry;
        return telemetryMessage;
    }

    applyDeltaInternal(parsed.delta, "applied");
    return telemetryMessage;
}

juce::String MagicTrackAudioProcessor::applyExternalDeltaJson(const juce::String& jsonResponse, const juce::String& sourceTag)
{
    auto parsed = juce::JSON::parse(jsonResponse);
    if (!parsed.isObject())
    {
        telemetryMessage = "applied: " + sourceTag + " invalid JSON";
        return telemetryMessage;
    }

    auto* root = parsed.getDynamicObject();
    if (root == nullptr)
    {
        telemetryMessage = "applied: " + sourceTag + " invalid payload";
        return telemetryMessage;
    }

    const auto findPropertyByName = [](juce::DynamicObject* object, const juce::String& key) -> juce::var {
        if (object == nullptr)
            return {};

        const auto& props = object->getProperties();
        for (int i = 0; i < props.size(); ++i)
        {
            if (props.getName(i).toString() == key)
                return props.getValueAt(i);
        }

        return {};
    };

    const auto sourceName = findPropertyByName(root, "source").toString();
    const auto reason = findPropertyByName(root, "reason").toString();
    const auto attemptedVar = findPropertyByName(root, "attempted");

    juce::String attemptedText;
    if (attemptedVar.isArray())
    {
        juce::StringArray items;
        for (const auto& v : *attemptedVar.getArray())
        {
            auto item = v.toString().trim();
            if (item.isNotEmpty())
                items.add(item);
        }

        if (!items.isEmpty())
            attemptedText = items.joinIntoString(",");
    }

    auto deltaPayload = findPropertyByName(root, "deltas");
    if (!deltaPayload.isObject())
        deltaPayload = findPropertyByName(root, "delta");
    if (!deltaPayload.isObject())
        deltaPayload = parsed;
    if (!deltaPayload.isObject())
    {
        telemetryMessage = "applied: " + sourceTag + " missing deltas";
        return telemetryMessage;
    }

    auto* deltaObj = deltaPayload.getDynamicObject();
    MacroState delta{};
    bool hasAnyDelta = false;

    for (int i = 0; i < kMacroCount; ++i)
    {
        const juce::String key = kMacroNames[static_cast<size_t>(i)];
        auto value = findPropertyByName(deltaObj, key);

        if (!value.isDouble() && !value.isInt() && !value.isInt64())
            continue;

        auto d = static_cast<float>(value);
        delta[static_cast<size_t>(i)] = d;
        if (std::abs(d) >= kDeltaEpsilon)
            hasAnyDelta = true;
    }

    if (!hasAnyDelta)
    {
        juce::String detail;
        if (sourceName.isNotEmpty())
            detail << " (" << sourceName << ")";
        if (attemptedText.isNotEmpty())
            detail << " attempted=" << attemptedText;
        if (reason.isNotEmpty() && reason != "ok")
            detail << " " << reason;
        telemetryMessage = "applied: " + sourceTag + detail + " returned no macro deltas";
        return telemetryMessage;
    }

    juce::String prefix = "applied (" + sourceTag;
    if (sourceName.isNotEmpty())
        prefix << ":" << sourceName;
    prefix << ")";
    if (attemptedText.isNotEmpty())
        prefix << " attempted=" << attemptedText;
    if (reason.isNotEmpty() && reason != "ok")
        prefix << " " << reason;

    applyDeltaInternal(delta, prefix);
    return telemetryMessage;
}

void MagicTrackAudioProcessor::undoLast()
{
    if (!hasUndoState)
        return;

    currentState = undoState;
    pushStateToParameters(currentState);
    telemetryMessage = "applied: undo last";
}

void MagicTrackAudioProcessor::storeSlotA()
{
    slotA = currentState;
    slotASet = true;
    telemetryMessage = "applied: stored A";
}

void MagicTrackAudioProcessor::recallSlotA()
{
    if (!slotASet)
        return;

    undoState = currentState;
    hasUndoState = true;
    currentState = slotA;
    pushStateToParameters(currentState);
    telemetryMessage = "applied: recalled A";
}

void MagicTrackAudioProcessor::storeSlotB()
{
    slotB = currentState;
    slotBSet = true;
    telemetryMessage = "applied: stored B";
}

void MagicTrackAudioProcessor::recallSlotB()
{
    if (!slotBSet)
        return;

    undoState = currentState;
    hasUndoState = true;
    currentState = slotB;
    pushStateToParameters(currentState);
    telemetryMessage = "applied: recalled B";
}

juce::AudioProcessorValueTreeState::ParameterLayout MagicTrackAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.reserve(kMacroCount + 1);

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("profile", 1),
        "profile",
        juce::StringArray{ "General", "Guitar", "Vocal", "Upright Bass" },
        0));

    for (int i = 0; i < kMacroCount; ++i)
    {
        auto name = juce::String(kMacroNames[static_cast<size_t>(i)]);
        auto defaultValue = kDefaultState[static_cast<size_t>(i)];

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(name, 1),
            name,
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f),
            defaultValue));
    }

    return { params.begin(), params.end() };
}

void MagicTrackAudioProcessor::syncStateFromParameters()
{
    for (int i = 0; i < kMacroCount; ++i)
    {
        auto name = macroParamId(static_cast<MacroId>(i));
        if (auto* raw = apvts.getRawParameterValue(name))
            currentState[static_cast<size_t>(i)] = raw->load();
    }

    clampAll(currentState);
}

void MagicTrackAudioProcessor::pushStateToParameters(const MacroState& state)
{
    for (int i = 0; i < kMacroCount; ++i)
    {
        auto name = macroParamId(static_cast<MacroId>(i));
        if (auto* param = apvts.getParameter(name))
            param->setValueNotifyingHost(state[static_cast<size_t>(i)] / 100.0f);
    }
}

void MagicTrackAudioProcessor::applyDeltaInternal(const MacroState& delta, const juce::String& telemetryPrefix)
{
    undoState = currentState;
    hasUndoState = true;

    for (int i = 0; i < kMacroCount; ++i)
        currentState[static_cast<size_t>(i)] += delta[static_cast<size_t>(i)];

    clampAll(currentState);
    pushStateToParameters(currentState);
    telemetryMessage = buildDeltaTelemetry(delta, telemetryPrefix);
}

juce::String MagicTrackAudioProcessor::buildDeltaTelemetry(const MacroState& delta, const juce::String& prefix)
{
    juce::StringArray parts;
    for (int i = 0; i < kMacroCount; ++i)
    {
        const auto value = delta[static_cast<size_t>(i)];
        if (std::abs(value) < kDeltaEpsilon)
            continue;

        const auto rounded = juce::roundToInt(value);
        const auto sign = rounded >= 0 ? "+" : "";
        parts.add(juce::String(kMacroNames[static_cast<size_t>(i)]) + " " + sign + juce::String(rounded));
    }

    if (parts.isEmpty())
        return prefix + ": no macro deltas";

    return prefix + ": " + parts.joinIntoString(", ");
}

float MagicTrackAudioProcessor::macro(MacroId id) const
{
    return currentState[static_cast<size_t>(id)];
}

int MagicTrackAudioProcessor::profileIndex() const
{
    if (auto* raw = apvts.getRawParameterValue("profile"))
        return juce::jlimit(0, 3, static_cast<int>(std::lround(raw->load())));

    return 0;
}

MagicTrackAudioProcessor::Profile MagicTrackAudioProcessor::getProfile() const
{
    return static_cast<Profile>(profileIndex());
}

void MagicTrackAudioProcessor::updateDspFromMacros()
{
    auto sampleRate = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
    const auto tuning = tuningForProfile(profileIndex());

    auto levelDb = normalizedToRange(macro(MacroId::level), tuning.levelMinDb, tuning.levelMaxDb);
    trim.setGainDecibels(levelDb);

    auto bodyGain = normalizedToRange(macro(MacroId::body), tuning.lowShelfMinDb, tuning.lowShelfMaxDb);
    *lowShelf.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf(sampleRate, tuning.lowShelfFrequency, 0.707f, juce::Decibels::decibelsToGain(bodyGain));

    auto brightGain = normalizedToRange(macro(MacroId::brightness), tuning.highShelfMinDb, tuning.highShelfMaxDb);
    *highShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, tuning.highShelfFrequency, 0.707f, juce::Decibels::decibelsToGain(brightGain));

    auto harshCutDb = normalizedToRange(macro(MacroId::harshness), tuning.harshEqAtMinMacroDb, tuning.harshEqAtMaxMacroDb);
    *harshPeak.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, tuning.harshFrequency, tuning.harshQ, juce::Decibels::decibelsToGain(harshCutDb));

    // Coarse dynamic harshness stage: higher macro values compress less; lower values tame upper-mid transients.
    harshCompressor.setThreshold(normalizedToRange(macro(MacroId::harshness), tuning.harshCompThresholdMin, tuning.harshCompThresholdMax));
    harshCompressor.setRatio(normalizedToRange(macro(MacroId::harshness), tuning.harshCompRatioMin, tuning.harshCompRatioMax));
    harshCompressor.setAttack(1.0f);
    harshCompressor.setRelease(60.0f);

    compressor.setThreshold(normalizedToRange(macro(MacroId::tighten), tuning.compThresholdMin, tuning.compThresholdMax));
    compressor.setRatio(normalizedToRange(macro(MacroId::tighten), tuning.compRatioMin, tuning.compRatioMax));
    compressor.setAttack(normalizedToRange(macro(MacroId::tighten), tuning.compAttackMaxMs, tuning.compAttackMinMs));
    compressor.setRelease(normalizedToRange(macro(MacroId::tighten), tuning.compReleaseMaxMs, tuning.compReleaseMinMs));

    saturatorDrive.setGainDecibels(normalizedToRange(macro(MacroId::crunch), tuning.saturatorDriveMinDb, tuning.saturatorDriveMaxDb));

    juce::dsp::Reverb::Parameters rev;
    rev.roomSize = normalizedToRange(macro(MacroId::space), tuning.reverbRoomMin, tuning.reverbRoomMax);
    rev.damping = tuning.reverbDamping;
    rev.width = 1.0f;
    rev.wetLevel = 1.0f;
    rev.dryLevel = 0.0f;
    rev.freezeMode = 0.0f;
    reverb.setParameters(rev);
}

} // namespace magictrack

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new magictrack::MagicTrackAudioProcessor();
}
