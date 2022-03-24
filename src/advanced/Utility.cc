#include "Utility.hh"

#include <cassert>
#include <cstdlib>
#include <cstring>

#include <mutex>

#include <imgui/imgui.h>

#include <typed-geometry/tg.hh>

namespace
{
constexpr uint64_t hash_combine(uint64_t a, uint64_t b) noexcept { return a * 6364136223846793005ULL + b + 0xda3e39cb94b95bdbULL; }

constexpr uint64_t stringhash(char const* s)
{
    if (!s)
        return 0;

    uint64_t h = 0x2a5114b5c6133408uLL;
    while (*s)
    {
        h = hash_combine(h, uint64_t(*s));
        s++;
    }
    return h;
}

struct statistic_slot
{
    char const* identifier = nullptr;
    uint64_t identifier_hash = 0;
    float* buffer = nullptr;
    float frame_value = 0.0;
    gamedev::StatType type = gamedev::StatType::NONE;
};
}

struct gamedev::StatisticsState
{
    std::mutex mtx;
    float* full_buffer = nullptr;
    statistic_slot* stats = nullptr;

    unsigned num_frames;
    unsigned max_num_stats;

    unsigned num_stats = 0;
};

gamedev::StatisticsState* gamedev::initializeStatistics(uint32_t maxNumStats, uint32_t numFrames)
{
    // this is ported, usually this wouldn't use operator new
    // (but it's alright here)
    StatisticsState* const res = new StatisticsState();

    res->full_buffer = new float[maxNumStats * numFrames];
    std::memset(res->full_buffer, 0, sizeof(float) * maxNumStats * numFrames);

    res->stats = new statistic_slot[maxNumStats]();

    for (auto i = 0u; i < maxNumStats; ++i)
    {
        statistic_slot& slot = res->stats[i];
        slot.buffer = res->full_buffer + (i * numFrames);
    }

    res->num_frames = numFrames;
    res->max_num_stats = maxNumStats;

    return res;
}

void gamedev::destroyStatistics(StatisticsState* state)
{
    // see above
    delete[] state->stats;
    delete[] state->full_buffer;
    delete state;
}

void gamedev::newStatFrame(StatisticsState* state)
{
    //  OPTICK_EVENT();
    auto lg = std::lock_guard(state->mtx);
    auto const moveSize = sizeof(float) * (state->num_frames - 1);

    for (auto i = 0u; i < state->num_stats; ++i)
    {
        statistic_slot& slot = state->stats[i];

        std::memmove(slot.buffer, slot.buffer + 1, moveSize);
        slot.buffer[state->num_frames - 1] = slot.frame_value;
        slot.frame_value = 0.0;
    }
}

float* gamedev::getStat(StatisticsState* state, char const* identifier, StatType type)
{
    // OPTICK_EVENT();
    auto const identifier_hash = stringhash(identifier);

    auto lg = std::lock_guard(state->mtx);

    // check existing stats
    for (auto i = 0u; i < state->num_stats; ++i)
    {
        if (state->stats[i].identifier_hash == identifier_hash)
        {
            assert(state->stats[i].type == type && "Requested stat has mismatching type");
            return &state->stats[i].frame_value;
        }
    }

    // create new stat
    assert(state->num_stats < state->max_num_stats && "Stat counters full");
    statistic_slot& new_slot = state->stats[state->num_stats++];

    new_slot.identifier = identifier;
    new_slot.identifier_hash = identifier_hash;
    new_slot.type = type;

    return &new_slot.frame_value;
}

void gamedev::runStatImgui(StatisticsState* state)
{
    // OPTICK_EVENT();

    auto lg = std::lock_guard(state->mtx);

    char imgui_label_buf[1024];

    if (ImGui::Begin("Statistics"))
    {
        if (ImGui::TreeNodeEx("Timings", ImGuiTreeNodeFlags_DefaultOpen))
        {
            for (auto i = 0u; i < state->num_stats; ++i)
            {
                auto const& stat = state->stats[i];
                if (stat.type != StatType::timing)
                    continue;

                std::snprintf(imgui_label_buf, sizeof(imgui_label_buf), "##%s Graph", stat.identifier);

                ImGui::PushID(i);
                ImGuiValueGraph(stat.buffer, state->num_frames, stat.identifier, imgui_label_buf, "ms", 300, 65);
                ImGui::PopID();
            }

            ImGui::TreePop();
        }

        if (ImGui::TreeNodeEx("Counters", ImGuiTreeNodeFlags_DefaultOpen))
        {
            for (auto i = 0u; i < state->num_stats; ++i)
            {
                auto const& stat = state->stats[i];
                if (stat.type != StatType::counter)
                    continue;

                std::snprintf(imgui_label_buf, sizeof(imgui_label_buf), "##%s Graph", stat.identifier);

                ImGui::PushID(i);
                ImGuiValueGraph(stat.buffer, state->num_frames, stat.identifier, imgui_label_buf, "", 300, 65, true);
                ImGui::PopID();
            }

            ImGui::TreePop();
        }
    }

    ImGui::End();
}

void gamedev::ImGuiValueGraph(float const* values, uint32_t numValues, char const* mainText, char const* label, char const* unit, float sizeX, float sizeY, bool startAtZero)
{
    auto const cpos_before_plot = ImGui::GetCursorScreenPos();

    auto const line_color = ImGui::GetColorU32(ImVec4(0.25f, 0.25f, 0.25f, 1.f));
    auto const text_color = ImGui::GetColorU32(ImVec4(0.6f, 0.6f, 0.6f, 1.f));

    float val_min = 999999.f;
    float val_max = -99999.f;
    float val_sum = 0.f;

    for (auto i = 0u; i < numValues; ++i)
    {
        float const val = values[i];

        val_sum += val;
        if (val < val_min)
        {
            val_min = val;
        }
        if (val > val_max)
        {
            val_max = val;
        }
    }

    float const plot_range_start = startAtZero ? 0.f : tg::min(tg::floor(val_min / 5.f) * 5.f, tg::max(val_min - 10.f, 0.f));
    float const plot_range_end = tg::max(tg::ceil(val_max / 5.f) * 5.f, plot_range_start + 10.f);

    char graph_label[128];

    ImVec2 plotSize = {sizeX, sizeY};

    std::snprintf(graph_label, sizeof(graph_label), "%1.f%s", plot_range_end, unit);
    ImGui::GetWindowDrawList()->AddText(ImVec2(cpos_before_plot.x + plotSize.x + 2, cpos_before_plot.y), text_color, graph_label);

    std::snprintf(graph_label, sizeof(graph_label), "%1.f%s", plot_range_start, unit);
    ImGui::GetWindowDrawList()->AddText(ImVec2(cpos_before_plot.x + plotSize.x + 2, cpos_before_plot.y + plotSize.y - 10), text_color, graph_label);

    std::snprintf(graph_label, sizeof(graph_label), "%.1f%s - %.1f%s", val_min, unit, val_max, unit);
    ImGui::GetWindowDrawList()->AddText(ImVec2(cpos_before_plot.x + plotSize.x + 6, cpos_before_plot.y + 16), text_color, graph_label);

    std::snprintf(graph_label, sizeof(graph_label), "avg %.2f%s", val_sum / numValues, unit);
    ImGui::GetWindowDrawList()->AddText(ImVec2(cpos_before_plot.x + plotSize.x + 6, cpos_before_plot.y + 27), text_color, graph_label);

    std::snprintf(graph_label, sizeof(graph_label), "curr %.2f%s", values[numValues - 1], unit);
    ImGui::GetWindowDrawList()->AddText(ImVec2(cpos_before_plot.x + plotSize.x + 6, cpos_before_plot.y + 40), text_color, graph_label);

    ImGui::PlotLines(label, values, numValues, 0, mainText, plot_range_start, plot_range_end, plotSize);
}
