#include "TextCommandParser.h"

#include <juce_core/juce_core.h>

#include <cmath>
#include <iostream>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: magictrack_parser_cli \"brighter but not harsh\"\n";
        return 1;
    }

    juce::File jsonFile = juce::File::getCurrentWorkingDirectory().getChildFile("Resources/phrases.v1.json");
    if (!jsonFile.existsAsFile())
    {
        std::cerr << "could not find Resources/phrases.v1.json from current directory\n";
        return 2;
    }

    magictrack::TextCommandParser parser;
    if (!parser.loadFromJsonString(jsonFile.loadFileAsString()))
    {
        std::cerr << "failed to parse phrase json\n";
        return 3;
    }

    auto input = juce::String::fromUTF8(argv[1]);
    auto parsed = parser.parse(input);

    std::cout << parsed.telemetry << "\n";
    if (!parsed.matched)
        return 0;

    for (int i = 0; i < magictrack::kMacroCount; ++i)
    {
        auto delta = parsed.delta[static_cast<size_t>(i)];
        if (std::abs(delta) < 0.001f)
            continue;

        std::cout << magictrack::kMacroNames[static_cast<size_t>(i)] << ": " << delta << "\n";
    }

    return 0;
}
