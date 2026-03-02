#pragma once

#include "MacroTypes.h"

#include <juce_core/juce_core.h>
#include <set>

namespace magictrack
{
struct PhraseEntry
{
    juce::String id;
    juce::StringArray matchAny;
    MacroState delta{};
    bool resetAll = false;
};

struct ParseResult
{
    bool matched = false;
    bool resetToDefaults = false;
    MacroState delta{};
    juce::StringArray matchedPhraseIds;
    juce::String telemetry;
};

class TextCommandParser
{
public:
    TextCommandParser();

    bool loadFromJsonString(const juce::String& jsonText);
    ParseResult parse(const juce::String& command) const;

private:
    struct IntensityRule
    {
        juce::String phrase;
        float factor = 1.0f;
    };

    struct Segment
    {
        juce::String original;
        juce::String normalized;
        float intensity = 1.0f;
        bool hasNegation = false;
    };

    static juce::String normalize(const juce::String& input);
    static juce::String normalizeForMatch(const juce::String& input);
    static bool containsStandalone(const juce::String& haystack, const juce::String& needle);

    static juce::Array<Segment> splitSegments(const juce::String& command);
    float detectIntensity(const Segment& segment) const;
    bool detectCapsBoost(const Segment& segment) const;

    void addHeuristicNegationIntent(const Segment& segment, ParseResult& result, std::set<juce::String>& alreadyMatched) const;

    void addDelta(ParseResult& result, const MacroState& delta, float scale, const juce::String& reason) const;
    juce::String buildTelemetry(const ParseResult& result) const;

    juce::Array<PhraseEntry> phrases;
    juce::Array<IntensityRule> intensityRules;
    juce::StringArray negationWords;
    juce::StringArray conjunctionWords;
    float capsBoostFactor = 1.25f;
    bool capsBoostEnabled = true;
};
} // namespace magictrack
