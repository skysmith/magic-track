#include "TextCommandParser.h"

#include <algorithm>

namespace
{
constexpr float kEpsilon = 0.01f;
}

namespace magictrack
{
TextCommandParser::TextCommandParser()
{
    intensityRules.add({ "a little", 0.5f });
    intensityRules.add({ "slightly", 0.5f });
    intensityRules.add({ "bit", 0.5f });
    intensityRules.add({ "somewhat", 0.75f });
    intensityRules.add({ "very", 1.25f });
    intensityRules.add({ "really", 1.5f });
    intensityRules.add({ "way", 1.5f });
    intensityRules.add({ "much", 1.5f });
    intensityRules.add({ "super", 2.0f });

    negationWords.addArray({ "not", "without", "less", "no" });
    conjunctionWords.addArray({ " and ", " but ", " however ", " though " });
}

bool TextCommandParser::loadFromJsonString(const juce::String& jsonText)
{
    auto parsed = juce::JSON::parse(jsonText);
    if (!parsed.isObject())
        return false;

    phrases.clear();

    if (auto* root = parsed.getDynamicObject())
    {
        const auto propertyByName = [](juce::DynamicObject* object, const juce::String& wantedName) -> juce::var {
            if (object == nullptr)
                return {};

            const auto& props = object->getProperties();
            for (int i = 0; i < props.size(); ++i)
            {
                if (props.getName(i).toString() == wantedName)
                    return props.getValueAt(i);
            }

            return {};
        };

        if (auto modifiers = propertyByName(root, "modifiers"); modifiers.isObject())
        {
            if (auto* modifierObj = modifiers.getDynamicObject())
            {
                if (auto intensity = propertyByName(modifierObj, "intensity_words"); intensity.isObject())
                {
                    intensityRules.clear();
                    if (auto* intensityObj = intensity.getDynamicObject())
                    {
                        const auto& properties = intensityObj->getProperties();
                        for (int i = 0; i < properties.size(); ++i)
                        {
                            const auto key = properties.getName(i).toString();
                            auto factor = static_cast<float>(intensityObj->getProperty(key));
                            if (key.isNotEmpty())
                                intensityRules.add({ key.toLowerCase(), factor });
                        }
                    }
                }

                if (auto negation = propertyByName(modifierObj, "negation_words"); negation.isArray())
                {
                    negationWords.clear();
                    for (const auto& item : *negation.getArray())
                        negationWords.add(item.toString().toLowerCase());
                }

                if (auto conjunction = propertyByName(modifierObj, "conjunctions"); conjunction.isArray())
                {
                    conjunctionWords.clear();
                    for (const auto& item : *conjunction.getArray())
                        conjunctionWords.add(" " + item.toString().toLowerCase() + " ");
                }

                if (auto caps = propertyByName(modifierObj, "caps_boost"); caps.isObject())
                {
                    if (auto* capsObj = caps.getDynamicObject())
                    {
                        capsBoostEnabled = static_cast<bool>(capsObj->getProperty("enabled"));
                        capsBoostFactor = static_cast<float>(capsObj->getProperty("factor"));
                    }
                }
            }
        }

        if (auto phrasesArray = propertyByName(root, "phrases"); phrasesArray.isArray())
        {
            for (const auto& item : *phrasesArray.getArray())
            {
                if (!item.isObject())
                    continue;

                auto* phraseObj = item.getDynamicObject();
                PhraseEntry entry;
                entry.id = propertyByName(phraseObj, "id").toString();

                if (auto matchAny = propertyByName(phraseObj, "match_any"); matchAny.isArray())
                {
                    for (const auto& alias : *matchAny.getArray())
                        entry.matchAny.add(normalizeForMatch(alias.toString()));
                }

                if (auto deltaVar = propertyByName(phraseObj, "delta"); deltaVar.isObject())
                {
                    auto* deltaObj = deltaVar.getDynamicObject();
                    for (int i = 0; i < kMacroCount; ++i)
                    {
                        auto name = juce::Identifier(kMacroNames[static_cast<size_t>(i)]);
                        if (deltaObj->hasProperty(name))
                            entry.delta[static_cast<size_t>(i)] = static_cast<float>(deltaObj->getProperty(name));
                    }
                }

                if (auto special = propertyByName(phraseObj, "special"); special.toString() == "reset_all_macros_to_default")
                    entry.resetAll = true;

                phrases.add(std::move(entry));
            }
        }
    }

    std::sort(phrases.begin(), phrases.end(), [](const PhraseEntry& a, const PhraseEntry& b) {
        int aLongest = 0;
        int bLongest = 0;
        for (const auto& alias : a.matchAny)
            aLongest = juce::jmax(aLongest, alias.length());
        for (const auto& alias : b.matchAny)
            bLongest = juce::jmax(bLongest, alias.length());
        return aLongest > bLongest;
    });

    return !phrases.isEmpty();
}

ParseResult TextCommandParser::parse(const juce::String& command) const
{
    ParseResult result;
    std::set<juce::String> alreadyMatched;

    auto segments = splitSegments(command);
    for (auto segment : segments)
    {
        segment.intensity = detectIntensity(segment);
        segment.hasNegation = false;
        for (const auto& negation : negationWords)
        {
            if (containsStandalone(segment.normalized, negation))
            {
                segment.hasNegation = true;
                break;
            }
        }

        bool matchedInSegment = false;
        for (const auto& phrase : phrases)
        {
            if (alreadyMatched.find(phrase.id) != alreadyMatched.end())
                continue;

            bool matchedAlias = false;
            for (const auto& alias : phrase.matchAny)
            {
                if (containsStandalone(segment.normalized, alias))
                {
                    matchedAlias = true;
                    break;
                }
            }

            if (!matchedAlias)
                continue;

            matchedInSegment = true;
            alreadyMatched.insert(phrase.id);
            result.matchedPhraseIds.add(phrase.id);
            result.matched = true;

            if (phrase.resetAll)
            {
                result.resetToDefaults = true;
                result.delta = {};
                result.telemetry = "applied: reset to defaults";
                return result;
            }

            addDelta(result, phrase.delta, segment.intensity, phrase.id);
        }

        if (!matchedInSegment && segment.hasNegation)
            addHeuristicNegationIntent(segment, result, alreadyMatched);
    }

    if (result.matched)
        result.telemetry = buildTelemetry(result);
    else
        result.telemetry = "applied: no phrase matched";

    return result;
}

juce::String TextCommandParser::normalize(const juce::String& input)
{
    auto lowered = input.toLowerCase().trim();
    juce::String out;
    out.preallocateBytes(lowered.length() + 1);

    for (auto ch : lowered)
    {
        if (juce::CharacterFunctions::isLetterOrDigit(ch) || ch == ' ' || ch == '!')
            out << ch;
        else
            out << ' ';
    }

    while (out.contains("  "))
        out = out.replace("  ", " ");

    return out.trim();
}

juce::String TextCommandParser::normalizeForMatch(const juce::String& input)
{
    auto normalized = normalize(input);
    return " " + normalized + " ";
}

bool TextCommandParser::containsStandalone(const juce::String& haystack, const juce::String& needle)
{
    return (" " + haystack + " ").contains(needle.startsWithChar(' ') ? needle : (" " + needle + " "));
}

juce::Array<TextCommandParser::Segment> TextCommandParser::splitSegments(const juce::String& command)
{
    juce::Array<Segment> segments;
    auto normalized = normalize(command);
    if (normalized.isEmpty())
        return segments;

    juce::String mutated = " " + normalized + " ";
    for (const juce::String word : { juce::String(" and "), juce::String(" but "), juce::String(" however "), juce::String(" though ") })
        mutated = mutated.replace(word, " | ");

    juce::StringArray pieces;
    pieces.addTokens(mutated, "|", "\"");
    pieces.trim();
    pieces.removeEmptyStrings();

    for (const auto& piece : pieces)
        segments.add({ command, piece.trim(), 1.0f, false });

    if (segments.isEmpty())
        segments.add({ command, normalized, 1.0f, false });

    return segments;
}

float TextCommandParser::detectIntensity(const Segment& segment) const
{
    float intensity = 1.0f;

    for (const auto& rule : intensityRules)
    {
        if (containsStandalone(segment.normalized, rule.phrase))
            intensity = juce::jmax(intensity, rule.factor);
    }

    if (detectCapsBoost(segment))
        intensity *= capsBoostFactor;

    return intensity;
}

bool TextCommandParser::detectCapsBoost(const Segment& segment) const
{
    if (!capsBoostEnabled)
        return false;

    if (segment.original.contains("!!!"))
        return true;

    juce::StringArray rawTokens;
    rawTokens.addTokens(segment.original, " ", "\"");

    for (auto token : rawTokens)
    {
        token = token.retainCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
        if (token.length() >= 2)
            return true;
    }

    return false;
}

void TextCommandParser::addHeuristicNegationIntent(const Segment& segment, ParseResult& result, std::set<juce::String>& alreadyMatched) const
{
    auto addIf = [&](const juce::String& phraseId, int macroIndex, float delta) {
        if (alreadyMatched.find(phraseId) != alreadyMatched.end())
            return;

        alreadyMatched.insert(phraseId);
        result.matched = true;
        result.matchedPhraseIds.add(phraseId);

        MacroState d{};
        d[static_cast<size_t>(macroIndex)] = delta;
        addDelta(result, d, segment.intensity, phraseId);
    };

    if (segment.normalized.contains("harsh"))
        addIf("heuristic_harsh_down", static_cast<int>(MacroId::harshness), -10.0f);

    if (segment.normalized.contains("crunch") || segment.normalized.contains("distort") || segment.normalized.contains("grit"))
        addIf("heuristic_crunch_down", static_cast<int>(MacroId::crunch), -12.0f);

    if (segment.normalized.contains("bright") || segment.normalized.contains("sparkle") || segment.normalized.contains("shimmer"))
        addIf("heuristic_brightness_down", static_cast<int>(MacroId::brightness), -8.0f);

    if (segment.normalized.contains("wide") || segment.normalized.contains("stereo"))
        addIf("heuristic_width_down", static_cast<int>(MacroId::width), -10.0f);

    if (segment.normalized.contains("reverb") || segment.normalized.contains("delay") || segment.normalized.contains("space") || segment.normalized.contains("wet"))
        addIf("heuristic_space_down", static_cast<int>(MacroId::space), -12.0f);
}

void TextCommandParser::addDelta(ParseResult& result, const MacroState& delta, float scale, const juce::String&) const
{
    for (int i = 0; i < kMacroCount; ++i)
        result.delta[static_cast<size_t>(i)] += delta[static_cast<size_t>(i)] * scale;
}

juce::String TextCommandParser::buildTelemetry(const ParseResult& result) const
{
    juce::StringArray parts;
    for (int i = 0; i < kMacroCount; ++i)
    {
        auto value = result.delta[static_cast<size_t>(i)];
        if (std::abs(value) < kEpsilon)
            continue;

        auto rounded = juce::roundToInt(value);
        auto sign = rounded >= 0 ? "+" : "";
        parts.add(juce::String(kMacroNames[static_cast<size_t>(i)]) + " " + sign + juce::String(rounded));
    }

    if (parts.isEmpty())
        return "applied: no macro deltas";

    return "applied: " + parts.joinIntoString(", ");
}
} // namespace magictrack
