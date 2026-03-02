#pragma once

#include <array>

namespace magictrack
{
constexpr int kMacroCount = 8;

enum class MacroId : int
{
    level = 0,
    brightness,
    harshness,
    body,
    tighten,
    crunch,
    width,
    space
};

inline constexpr std::array<const char*, kMacroCount> kMacroNames = {
    "level",
    "brightness",
    "harshness",
    "body",
    "tighten",
    "crunch",
    "width",
    "space"
};

using MacroState = std::array<float, kMacroCount>;

inline constexpr MacroState kDefaultState = {
    50.0f,
    50.0f,
    50.0f,
    50.0f,
    50.0f,
    50.0f,
    50.0f,
    20.0f
};

inline float clampMacro(float value)
{
    if (value < 0.0f)
        return 0.0f;

    if (value > 100.0f)
        return 100.0f;

    return value;
}

inline void clampAll(MacroState& state)
{
    for (auto& value : state)
        value = clampMacro(value);
}
} // namespace magictrack
