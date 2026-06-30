#pragma once

#include "imgui.h"
#include "imgui_internal.h"

namespace fx
{
    void glow_rect(ImDrawList* dl, ImRect rect, ImVec4 color, float strength, float rounding);
    void glow_circle(ImDrawList* dl, ImVec2 center, float radius, ImVec4 color, float strength);
    void soft_shadow(ImDrawList* dl, ImRect rect, float rounding, float alpha = 0.35f);
    void gradient_text(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, const char* text, ImVec4 from, ImVec4 to);
    ImVec2 snap_pos(ImVec2 pos);
    void icon_text(ImDrawList* dl, ImFont* font, float size, ImVec2 center, const char* text, ImU32 col, float rotation_deg = 0.f);
}
