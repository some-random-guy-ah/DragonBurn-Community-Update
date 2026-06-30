#include "sidebar.h"

#include "helpers/animation.h"
#include "helpers/fonts.h"
#include "theme/colors.h"
#include "theme/layout.h"
#include "ui/draw/fx.h"

#include "imgui.h"

#include <map>

namespace sidebar
{
    const char* tab_name(tab t)
    {
        switch (t)
        {
        case tab::combat: return "Combat";
        case tab::visuals: return "Visuals";
        case tab::misc: return "Misc";
        case tab::settings: return "Settings";
        default: return "";
        }
    }

    namespace
    {
        struct TabAnim
        {
            float hover = 0.f;
            float active = 0.f;
        };

        std::map<ImGuiID, TabAnim> g_tab_anims;

        void draw_brand(ImDrawList* dl, float x, float y)
        {
            const ImRect box({ x, y }, { x + 36.f, y + 36.f });
            dl->AddRectFilled(box.Min, box.Max, ImGui::GetColorU32(colors::TabBox), layout::round);
            fx::glow_rect(dl, box, colors::Accent, 0.18f, layout::round);

            if (ImFont* brand = fonts::brand_18())
            {
                const char* letter = "C";
                const ImVec2 ts = brand->CalcTextSizeA(20.f, FLT_MAX, 0.f, letter);
                dl->AddText(brand, 20.f,
                    fx::snap_pos({ box.GetCenter().x - ts.x * 0.5f, box.GetCenter().y - ts.y * 0.5f }),
                    ImGui::GetColorU32(colors::Accent), letter);
            }

            if (ImFont* title = fonts::brand_18())
            {
                const ImVec2 ts = title->CalcTextSizeA(18.f, FLT_MAX, 0.f, "ESP");
                fx::gradient_text(
                    dl, title, 18.f,
                    fx::snap_pos({ x + 44.f, y + 10.f - ts.y * 0.5f }),
                    "ESP",
                    colors::AccentDark,
                    colors::AccentHover);
            }
        }

        void draw_tab_row(ImDrawList* dl, float x, float& y, float w, tab item, state& st)
        {
            const bool selected = (st.selected == item);
            const ImRect row({ x, y }, { x + w, y + 36.f });
            ImGuiID id = ImGui::GetID(tab_name(item));

            ImGui::ItemSize(row);
            ImGui::ItemAdd(row, id);

            bool hovered = false, held = false;
            const bool pressed = ImGui::ButtonBehavior(row, id, &hovered, &held);

            TabAnim& a = g_tab_anims[id];
            anim::ease(a.hover, hovered ? 1.f : 0.f, ImGui::GetIO().DeltaTime, 14.f);
            anim::ease(a.active, selected ? 1.f : 0.f, ImGui::GetIO().DeltaTime, 16.f);

            if (a.active > 0.01f || a.hover > 0.01f)
            {
                dl->AddRectFilled(
                    row.Min, row.Max,
                    ImGui::GetColorU32(ImVec4(colors::ActiveTab.x, colors::ActiveTab.y, colors::ActiveTab.z, 0.5f + a.hover * 0.12f)),
                    layout::round);
            }

            if (selected)
            {
                const float bar_x = row.Min.x + layout::accent_bar_x;
                dl->AddRectFilled(
                    { bar_x, row.Min.y + 10.f },
                    { bar_x + layout::accent_bar_w, row.Max.y - 10.f },
                    ImGui::GetColorU32(colors::Accent),
                    0.f);
            }

            const ImU32 col = ImGui::GetColorU32(anim::lerp_color(colors::TextDisabled, colors::Text, a.active + a.hover * 0.3f));
            if (ImFont* font = fonts::regular_14())
            {
                const char* label = tab_name(item);
                const ImVec2 ts = font->CalcTextSizeA(16.f, FLT_MAX, 0.f, label);
                dl->AddText(font, 16.f,
                    fx::snap_pos({ x + 14.f, row.GetCenter().y - ts.y * 0.5f }),
                    col, label);
            }

            if (hovered)
                st.blocks_drag = true;
            if (pressed)
                st.selected = item;

            y += 38.f;
        }
    }

    void draw(const ImRect& bounds, state& sidebar_state)
    {
        sidebar_state.blocks_drag = false;

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(bounds.Min, bounds.Max, ImGui::GetColorU32(colors::SidebarBg), layout::round);

        const float pad = 12.f;
        const float inner_w = bounds.GetWidth() - pad * 2.f;
        float y = bounds.Min.y + pad;

        const float x = bounds.Min.x + pad;
        draw_brand(dl, x, y);
        y += 52.f;

        for (int i = 0; i < (int)tab::count; ++i)
            draw_tab_row(dl, x, y, inner_w, (tab)i, sidebar_state);
    }
}
