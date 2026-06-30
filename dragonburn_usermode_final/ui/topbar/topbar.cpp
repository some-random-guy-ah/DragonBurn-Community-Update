#include "topbar.h"

#include "helpers/animation.h"
#include "helpers/fonts.h"
#include "helpers/icons.h"
#include "theme/colors.h"
#include "theme/layout.h"
#include "ui/draw/fx.h"

#include "imgui.h"

#include <map>

namespace topbar
{
    const char* combat_tab_name(combat_tab tab)
    {
        switch (tab)
        {
        case combat_tab::aimbot: return "Aimbot";
        case combat_tab::triggerbot: return "Triggerbot";
        case combat_tab::recoil: return "Recoil";
        default: return "";
        }
    }

    namespace
    {
        struct TabAnim
        {
            float pill_alpha = 0.f;
            ImVec4 text = colors::TabTextIdle;
        };

        std::map<ImGuiID, TabAnim> g_tab_anims;

        float calc_tab_width(const char* label, ImFont* label_font, float label_size)
        {
            const ImVec2 label_ts = label_font
                ? label_font->CalcTextSizeA(label_size, FLT_MAX, 0.f, label)
                : ImGui::CalcTextSize(label);
            return label_ts.x + 36.f;
        }

        float measure_tabs_strip_width()
        {
            ImFont* label_font = fonts::regular_14();
            const float label_size = 16.f;
            float w = 0.f;
            for (int i = 0; i < (int)combat_tab::count; ++i)
            {
                const combat_tab item = (combat_tab)i;
                w += calc_tab_width(combat_tab_name(item), label_font, label_size);
            }
            return w;
        }

        void draw_text_clipped_v(ImDrawList* dl, ImFont* font, float size, const ImRect& rect,
            ImU32 col, const char* text, ImVec2 align)
        {
            if (!font || !text)
                return;

            const ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, 0.f, text);
            const float x = rect.Min.x + (rect.GetWidth() - ts.x) * align.x;
            const float y = rect.Min.y + (rect.GetHeight() - ts.y) * align.y;
            dl->AddText(font, size, fx::snap_pos({ x, y }), col, text);
        }

        void draw_combat_tabs(ImDrawList* dl, const ImRect& strip, state& st)
        {
            const float strip_h = strip.GetHeight();
            const float pill_round = strip_h * 0.5f;
            dl->AddRectFilled(strip.Min, strip.Max, ImGui::GetColorU32(colors::TabStripBg), pill_round);

            ImFont* label_font = fonts::regular_14();
            const float label_size = 16.f;
            const float label_pad = 12.f;

            float x = strip.Min.x;
            for (int i = 0; i < (int)combat_tab::count; ++i)
            {
                const combat_tab item = (combat_tab)i;
                const bool selected = (st.selected == item);
                const char* label = combat_tab_name(item);

                const float tab_w = calc_tab_width(label, label_font, label_size);
                const ImRect tab_bb({ x, strip.Min.y }, { x + tab_w, strip.Max.y });

                ImGuiID id = ImGui::GetID(label);
                ImGui::ItemSize(tab_bb);
                ImGui::ItemAdd(tab_bb, id);

                bool hovered = false, held = false;
                const bool pressed = ImGui::ButtonBehavior(tab_bb, id, &hovered, &held);

                TabAnim& anim = g_tab_anims[id];
                anim::static_ease(anim.pill_alpha, selected ? 1.f : 0.f, 7.f);
                anim::dynamic_ease(anim.text, selected ? colors::TabTextActive : colors::TabTextIdle, 24.f);

                if (anim.pill_alpha > 0.001f)
                {
                    dl->AddRectFilled(
                        tab_bb.Min, tab_bb.Max,
                        ImGui::GetColorU32(ImVec4(
                            colors::TabPill.x, colors::TabPill.y, colors::TabPill.z,
                            colors::TabPill.w * anim.pill_alpha)),
                        pill_round);
                }

                const ImU32 text_col = ImGui::GetColorU32(anim.text);
                const ImRect label_rect(
                    { tab_bb.Min.x, tab_bb.Min.y },
                    { tab_bb.Max.x - label_pad, tab_bb.Max.y });

                draw_text_clipped_v(dl, label_font, label_size, label_rect, text_col, label, { 1.f, 0.5f });

                if (hovered)
                    st.blocks_drag = true;
                if (pressed)
                    st.selected = item;

                x += tab_w;
            }
        }
    }

    float draw_header(state& header_state)
    {
        header_state.blocks_drag = false;

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const float content_w = ImGui::GetContentRegionAvail().x;
        const float x = origin.x;
        const float row_h = layout::combat_tabs_h;

        const float tabs_w = measure_tabs_strip_width();
        const ImRect tab_strip({ x, origin.y }, { x + tabs_w, origin.y + row_h });
        draw_combat_tabs(dl, tab_strip, header_state);

        const float total_h = row_h + layout::header_gap;
        ImGui::Dummy({ content_w, total_h });
        return total_h;
    }
}
