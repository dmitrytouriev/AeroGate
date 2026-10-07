#pragma once

#include <JuceHeader.h>

namespace aerosound::ui
{
constexpr float designWidth = 1100.0f;
constexpr float designHeight = 760.0f;

constexpr juce::uint32 ink          = 0xff102333;
constexpr juce::uint32 mutedInk     = 0xff4b687c;
constexpr juce::uint32 titleInk     = 0xff52758b;
constexpr juce::uint32 accent       = 0xff68b9e8;
constexpr juce::uint32 accentStrong = 0xff2487c6;
constexpr juce::uint32 accentDark   = 0xff146aa5;
constexpr juce::uint32 lineBlue     = 0xff93caeb;
constexpr juce::uint32 warning24    = 0xffd08b4b;
constexpr juce::uint32 warning48    = 0xffdc5656;
constexpr juce::uint32 measuringRed = 0xffdf3f48;

constexpr juce::uint32 skyLeft      = 0xffd9f3ff;
constexpr juce::uint32 skyMidLeft   = 0xffc7ecfb;
constexpr juce::uint32 skyMidRight  = 0xffa0daf3;
constexpr juce::uint32 skyRight     = 0xff83c9ec;
constexpr juce::uint32 panelFill    = 0xffeffaff;

constexpr juce::uint32 meterTrack   = 0xff31556a;
constexpr juce::uint32 meterCyan    = 0xff20cfe8;
constexpr juce::uint32 meterGreen   = 0xff57dc71;
constexpr juce::uint32 meterYellow  = 0xfff0db45;
constexpr juce::uint32 meterOrange  = 0xffffa33b;
constexpr juce::uint32 meterRed     = 0xffe55252;

constexpr float panelRadius = 18.0f;
constexpr float valueBoxRadius = 4.0f;

constexpr float overlayDimAlpha = 0.12f;
constexpr float overlayShadowAlpha = 0.10f;
constexpr float overlayCardFillAlpha = 0.98f;
constexpr float overlayCardBorderAlpha = 0.86f;
constexpr float overlayCardRadius = 12.0f;

constexpr std::uint32_t tooltipDelayMs = 1200u;

inline float scaleFor(int width, int height) noexcept
{
    return juce::jmin(width / designWidth, height / designHeight);
}

inline juce::Font font(float size, int style = juce::Font::plain)
{
    return juce::Font(juce::FontOptions("Segoe UI", size, style));
}

inline juce::Font monoFont(float size, int style = juce::Font::plain)
{
    return juce::Font(juce::FontOptions("Courier New", size, style));
}
}
