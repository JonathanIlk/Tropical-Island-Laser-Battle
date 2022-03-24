#pragma once

#include <cstdint>

namespace gamedev
{
//
// Simple statistics system

struct StatisticsState;

enum class StatType
{
    NONE = 0,
    counter, // counter
    timing   // timer in milliseconds
};

StatisticsState* initializeStatistics(uint32_t maxNumStats, uint32_t numFrames);

void destroyStatistics(StatisticsState* state);

void newStatFrame(StatisticsState* state);

// obtain a stat pointer, simply add values to it over the frame (starts out at zero)
float* getStat(StatisticsState* state, char const* identifier, StatType type);

// run the stat imgui, showing graphs for all live stats
void runStatImgui(StatisticsState* state);


// Slightly more elaborate imgui graph than the default one
void ImGuiValueGraph(float const* values, uint32_t numValues, char const* mainText, char const* label, char const* unit, float sizeX, float sizeY, bool startAtZero = false);
}
