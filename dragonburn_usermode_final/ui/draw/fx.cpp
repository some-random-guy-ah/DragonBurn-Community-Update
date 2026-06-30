#include "fx.h"

#include "theme/colors.h"

#include <cmath>
#include <cstring>

namespace fx
{
    ImVec2 snap_pos(ImVec2 pos)
    {
        return { floorf(pos.x + 0.5f), floorf(pos.y + 0.5f) };
    }

    void gradient_text(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, const char* text, ImVec4 from, ImVec4 to)
    {
        if (!font || !text || !text[0])
            return;

        pos = snap_pos(pos);
        const int len = (int)strlen(text);
        if (len <= 0)
            return;

        float x = floorf(pos.x);
        const float y = floorf(pos.y);
        for (int i = 0; i < len; ++i)
        {
            const char c[2] = { text[i], '\0' };
            const float t = len > 1 ? (float)i / (float)(len - 1) : 0.f;
            const ImVec4 col(
                from.x + (to.x - from.x) * t,
                from.y + (to.y - from.y) * t,
                from.z + (to.z - from.z) * t,
                from.w + (to.w - from.w) * t);
            const ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, 0.f, c);
            dl->AddText(font, size, { x, y }, ImGui::GetColorU32(col), c);
            x += floorf(ts.x + 0.5f);
        }
    }

    void glow_rect(ImDrawList* dl, ImRect rect, ImVec4 color, float strength, float rounding)
    {
        if (strength <= 0.001f)
            return;

        for (int i = 3; i >= 1; --i)
        {
            const float expand = (float)i * 0.55f;
            const float alpha = (0.16f * strength) / (float)i;
            dl->AddRectFilled(
                rect.Min - ImVec2(expand, expand),
                rect.Max + ImVec2(expand, expand),
                ImGui::GetColorU32(ImVec4(color.x, color.y, color.z, alpha)),
                rounding + expand);
        }
    }

    void glow_circle(ImDrawList* dl, ImVec2 center, float radius, ImVec4 color, float strength)
    {
        if (strength <= 0.001f)
            return;

        for (int i = 2; i >= 1; --i)
        {
            const float expand = (float)i * 0.9f;
            const float alpha = (0.12f * strength) / (float)i;
            dl->AddCircleFilled(
                center, radius + expand,
                ImGui::GetColorU32(ImVec4(color.x, color.y, color.z, alpha)), 16);
        }
    }

    void soft_shadow(ImDrawList* dl, ImRect rect, float rounding, float alpha)
    {
        dl->AddRectFilled(
            rect.Min + ImVec2(0.f, 2.f),
            rect.Max + ImVec2(0.f, 3.f),
            ImGui::GetColorU32(ImVec4(0.f, 0.f, 0.f, alpha * 0.35f)),
            rounding);
    }

    void icon_text(ImDrawList* dl, ImFont* font, float size, ImVec2 center, const char* text, ImU32 col, float rotation_deg)
    {
        if (!font || !text || !text[0])
            return;

        const ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, 0.f, text);
        const ImVec2 pos = { center.x - ts.x * 0.5f, center.y - ts.y * 0.5f };

        if (fabsf(rotation_deg) < 0.01f)
        {
            dl->AddText(font, size, snap_pos(pos), col, text);
            return;
        }

        const int vtx_start = dl->VtxBuffer.Size;
        dl->AddText(font, size, pos, col, text);
        const int vtx_end = dl->VtxBuffer.Size;

        const float rad = rotation_deg * (IM_PI / 180.f);
        const float cos_a = cosf(rad);
        const float sin_a = sinf(rad);
        for (int i = vtx_start; i < vtx_end; ++i)
        {
            ImDrawVert& v = dl->VtxBuffer[i];
            const ImVec2 rel = v.pos - center;
            v.pos = {
                center.x + rel.x * cos_a - rel.y * sin_a,
                center.y + rel.x * sin_a + rel.y * cos_a };
        }
    }
}
