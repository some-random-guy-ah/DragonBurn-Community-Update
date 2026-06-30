#pragma once
#include <imgui.h>
#include "settings.h"
#include "types.h"
#include "utils.h"
#include "crosshair.h"
#include "grenades.h"
#include "overlay.h"
#include "weapon_icons.h"

#include "theme/colors.h"
#include "theme/layout.h"
#include "helpers/fonts.h"
#include "helpers/window_drag.h"
#include "ui/widgets/widgets.h"
#include "ui/sidebar/sidebar.h"
#include "ui/topbar/topbar.h"
#include "ui/draw/fx.h"

class Menu {
public:
    void toggle() { g_settings.menu_open = !g_settings.menu_open; }

    void render() {
        if (!g_settings.menu_open) return;

        ImGuiIO& io = ImGui::GetIO();
        const float win_w = layout::window_w;
        const float win_h = layout::window_h;
        const ImVec2 win_pos((io.DisplaySize.x - win_w) * 0.5f, (io.DisplaySize.y - win_h) * 0.5f);

        ImGui::SetNextWindowPos(win_pos, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({win_w, win_h}, ImGuiCond_FirstUseEver);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.f, 0.f});

        ImGui::Begin("##portfolio_menu", &g_settings.menu_open,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBackground);

        ImGui::PopStyleVar(3);

        const ImVec2 origin = ImGui::GetWindowPos();
        const ImRect window_rect(origin, origin + ImVec2(win_w, win_h));

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(window_rect.Min, window_rect.Max,
            ImGui::GetColorU32(colors::ShellTint), layout::shell_round);

        // Sidebar
        const float sidebar_w = 150.f;
        const ImRect sidebar_rect(origin, origin + ImVec2(sidebar_w, win_h));
        sidebar::draw(sidebar_rect, m_sidebar);

        // Content area
        const float content_x = origin.x + sidebar_w;
        const float content_w = win_w - sidebar_w;
        const float content_y = origin.y;
        const float content_h = win_h;

        ImGui::SetCursorScreenPos({content_x + layout::content_pad, content_y + layout::content_pad});
        ImGui::BeginChild("##content", {content_w - layout::content_pad * 2.f, content_h - layout::content_pad * 2.f},
            false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);

        render_page();

        ImGui::EndChild();

        window_drag::handle(nullptr, window_rect, m_sidebar.blocks_drag);

        ImGui::End();
    }

private:
    sidebar::state m_sidebar;
    topbar::state m_topbar;

    bool bind_waiting_aimbot = false;
    bool bind_waiting_trigger = false;
    bool reset_popup_open = false;
    int m_filter_idx = 0;
    static constexpr const char* nade_filter_items[] = {"All", "Smoke", "Molotov", "Frag", "Flash"};

    // ── Page router ────────────────────────────────────────────────

    void render_page() {
        const float avail_w = ImGui::GetContentRegionAvail().x;
        const float col_w = (avail_w - layout::column_gap) * 0.5f;

        switch (m_sidebar.selected) {
        case sidebar::tab::combat:   render_combat();   break;
        case sidebar::tab::visuals:  render_visuals(col_w);  break;
        case sidebar::tab::misc:     render_misc(col_w);     break;
        case sidebar::tab::settings: render_settings(col_w); break;
        }
    }

    // ── Combat (with subtabs) ──────────────────────────────────────

    void render_combat() {
        const float header_h = topbar::draw_header(m_topbar);
        ImGui::BeginChild("##combat_body",
            {ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y - header_h},
            false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);

        const float cw = ImGui::GetContentRegionAvail().x;

        switch (m_topbar.selected) {
        case topbar::combat_tab::aimbot:
            render_aimbot_panel(cw);
            break;
        case topbar::combat_tab::triggerbot:
            render_triggerbot_panel(cw);
            break;
        case topbar::combat_tab::recoil:
            render_recoil_panel(cw);
            break;
        }

        ImGui::EndChild();
    }

    void render_aimbot_panel(float cw) {
        int rows = 1 + (g_settings.aimbot_enabled ? 4 : 0);
        if (!widgets::begin_panel("Aimbot", cw, rows)) return;
        widgets::checkbox("Enable", &g_settings.aimbot_enabled);
        if (g_settings.aimbot_enabled) {
            render_key_bind("Key", g_settings.key_aimbot, bind_waiting_aimbot);
            widgets::slider_float("Field of View", &g_settings.aimbot_fov, 1, 360, "%.0f");
            widgets::slider_float("Smoothness", &g_settings.aimbot_smooth, 1.f, 20.f, "%.0f", true);
            const char* bones[] = { "Head", "Neck", "Chest", "Pelvis" };
            widgets::combo("Bone", &g_settings.aimbot_bone, bones, 4);
        }
        widgets::end_panel();
    }

    void render_triggerbot_panel(float cw) {
        int rows = 1 + (g_settings.triggerbot_enabled ? 4 : 0);
        if (!widgets::begin_panel("Triggerbot", cw, rows)) return;
        widgets::checkbox("Enable", &g_settings.triggerbot_enabled);
        if (g_settings.triggerbot_enabled) {
            render_key_bind("Trigger Key", g_settings.key_triggerbot, bind_waiting_trigger);
            widgets::checkbox("Always Active", &g_settings.triggerbot_always_on);
            widgets::checkbox("Scoped Only", &g_settings.triggerbot_scoped_only);
            widgets::slider_float("Delay (ms)", &g_settings.triggerbot_delay, 0, 300, "%.0f", true);
        }
        widgets::end_panel();
    }

    void render_recoil_panel(float cw) {
        if (!widgets::begin_panel("Recoil Control", cw, 1)) return;
        ImFont* f = fonts::regular();
        if (f) {
            const ImVec2 ts = f->CalcTextSizeA(15.f, FLT_MAX, 0.f, "Not implemented");
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddText(f, 15.f,
                {ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y + 30.f},
                ImGui::GetColorU32(colors::TextMuted), "Coming soon");
        }
        widgets::end_panel();
    }

    // ── Visuals ────────────────────────────────────────────────────

    void render_visuals(float col_w) {
        auto panel_esp_theme = [&]() {
            int rows = 1 + (g_settings.esp_use_theme ? 2 : 6);
            if (!widgets::begin_panel("ESP Theme", col_w, rows)) return;
            widgets::checkbox("Use Theme Color", &g_settings.esp_use_theme);
            if (g_settings.esp_use_theme) {
                ImGui::ColorEdit4("Theme##tc", g_settings.esp_theme_color,
                    ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
                struct Preset { const char* name; float r, g, b; };
                static constexpr Preset presets[] = {
                    {"Cyan", 0, 0.85f, 1}, {"Lime", 0.2f, 1, 0.3f},
                    {"Blue", 0.235f, 0.68f, 0.93f}, {"Magenta", 0.9f, 0.1f, 0.8f},
                    {"White", 0.95f, 0.95f, 0.95f}, {"Red", 1, 0.15f, 0.15f},
                };
                for (int i = 0; i < 6; i++) {
                    if (ImGui::Button(presets[i].name, {46, 0})) {
                        g_settings.esp_theme_color[0] = presets[i].r;
                        g_settings.esp_theme_color[1] = presets[i].g;
                        g_settings.esp_theme_color[2] = presets[i].b;
                        g_settings.esp_theme_color[3] = 1;
                    }
                    if (i < 5) ImGui::SameLine();
                }
            } else {
                ImGui::ColorEdit4("Enemy Fill##ef", g_settings.enemy_fill, ImGuiColorEditFlags_NoInputs);
                ImGui::ColorEdit4("Enemy Outline##eo", g_settings.enemy_outline, ImGuiColorEditFlags_NoInputs);
                ImGui::ColorEdit4("Enemy Glow##eg", g_settings.enemy_glow, ImGuiColorEditFlags_NoInputs);
                ImGui::ColorEdit4("Team Fill##tf", g_settings.team_fill, ImGuiColorEditFlags_NoInputs);
                ImGui::ColorEdit4("Team Outline##to", g_settings.team_outline, ImGuiColorEditFlags_NoInputs);
                ImGui::ColorEdit4("Team Glow##tg", g_settings.team_glow, ImGuiColorEditFlags_NoInputs);
            }
            widgets::end_panel();
        };

        auto panel_esp = [&]() {
            int rows = 6;
            if (!widgets::begin_panel("ESP", col_w, rows)) return;
            widgets::checkbox("Enabled", &g_settings.esp_enabled);
            widgets::checkbox("Draw Teammates", &g_settings.draw_teammates);
            widgets::checkbox("Draw Head", &g_settings.draw_head, true);
            widgets::checkbox("Show Scoped", &g_settings.show_is_scoped, true);
            widgets::checkbox("C4 ESP", &g_settings.c4_esp_enabled, true);
            widgets::checkbox("Bomb Timer", &g_settings.show_bomb_timer, true);
            widgets::end_panel();
        };

        auto panel_opacity = [&]() {
            int rows = 1 + (g_settings.esp_opacity_drop ? 3 : 0);
            if (!widgets::begin_panel("Distance Opacity", col_w, rows)) return;
            widgets::checkbox("Enabled##opdrop", &g_settings.esp_opacity_drop);
            if (g_settings.esp_opacity_drop) {
                widgets::slider_float("Fade Start", &g_settings.esp_opacity_drop_start, 100, 3000, "%.0f");
                widgets::slider_float("Fade End", &g_settings.esp_opacity_drop_end, 200, 5000, "%.0f");
                widgets::slider_float("Min Opacity", &g_settings.esp_opacity_drop_min, 0, 1, "%.2f", true);
            }
            widgets::end_panel();
        };

        auto panel_chams = [&]() {
            int rows = 1 + (g_settings.chams_enabled ? 1 : 0);
            if (!widgets::begin_panel("Chams", col_w, rows)) return;
            widgets::checkbox("Enabled##chams", &g_settings.chams_enabled);
            if (g_settings.chams_enabled) {
                static const char* styles[] = {"Filled", "Wire", "Glow", "Skeleton"};
                widgets::combo("Style", &g_settings.chams_style, styles, 4, true);
            }
            widgets::end_panel();
        };

        auto panel_box = [&]() {
            int rows = 1 + (g_settings.draw_box ? 5 : 0);
            if (!widgets::begin_panel("Box ESP", col_w, rows)) return;
            widgets::checkbox("Show Box", &g_settings.draw_box);
            if (g_settings.draw_box) {
                static const char* box_styles[] = {"Corners", "Full", "Dashed"};
                widgets::combo("Style", &g_settings.box_style, box_styles, 3);
                widgets::slider_float("Thickness", &g_settings.box_thickness, 0.5f, 4.f, "%.1f");
                widgets::slider_float("Pad X", &g_settings.box_padding_x, 0.f, 20.f, "%.1f");
                widgets::slider_float("Pad Y", &g_settings.box_padding_y, 0.f, 20.f, "%.1f");
                if (g_settings.box_style == 0)
                    widgets::slider_float("Corner %", &g_settings.box_corner_pct, 0.1f, 0.5f, "%.2f");
            }
            widgets::end_panel();
        };

        auto panel_health = [&]() {
            int rows = 2 + (g_settings.draw_healthbar ? 2 : 0) + (g_settings.draw_health_text ? 3 : 0);
            if (!widgets::begin_panel("Health", col_w, rows)) return;
            widgets::checkbox("Health Bar", &g_settings.draw_healthbar);
            if (g_settings.draw_healthbar) {
                widgets::checkbox("Solid Color##hbsc", &g_settings.healthbar_solid_color);
                if (g_settings.healthbar_solid_color && !g_settings.esp_use_theme)
                    ImGui::ColorEdit4("Bar Color##hbcol", g_settings.healthbar_color, ImGuiColorEditFlags_NoInputs);
            }
            widgets::checkbox("Health Text", &g_settings.draw_health_text);
            if (g_settings.draw_health_text) {
                if (!g_settings.esp_use_theme)
                    ImGui::ColorEdit4("HP Color##hptc", g_settings.hp_text_color, ImGuiColorEditFlags_NoInputs);
                widgets::checkbox("HP Shadow##hpsh", &g_settings.hp_text_shadow);
                if (g_settings.hp_text_shadow)
                    ImGui::ColorEdit4("Shadow##hpsc", g_settings.hp_text_shadow_color, ImGuiColorEditFlags_NoInputs);
            }
            widgets::end_panel();
        };

        auto panel_name = [&]() {
            int rows = 1 + (g_settings.draw_name ? 5 : 0);
            if (!widgets::begin_panel("Name", col_w, rows)) return;
            widgets::checkbox("Show Name", &g_settings.draw_name);
            if (g_settings.draw_name) {
                static const char* pos[] = {"Top", "Bottom"};
                widgets::combo("Position", &g_settings.name_position, pos, 2);
                widgets::slider_float("Offset X", &g_settings.name_offset_x, -50, 50, "%.1f");
                widgets::slider_float("Offset Y", &g_settings.name_offset_y, -50, 50, "%.1f");
                if (!g_settings.esp_use_theme)
                    ImGui::ColorEdit4("Name Color##nc", g_settings.name_color, ImGuiColorEditFlags_NoInputs);
                widgets::checkbox("Shadow##nmsh", &g_settings.name_shadow);
                if (g_settings.name_shadow)
                    ImGui::ColorEdit4("Shadow##nmsc", g_settings.name_shadow_color, ImGuiColorEditFlags_NoInputs);
            }
            widgets::end_panel();
        };

        auto panel_weapon = [&]() {
            int rows = 1 + (g_settings.draw_weapon ? 6 : 0);
            if (!widgets::begin_panel("Weapon", col_w, rows)) return;
            widgets::checkbox("Show Weapon", &g_settings.draw_weapon);
            if (g_settings.draw_weapon) {
                widgets::checkbox("Show Icon", &g_settings.weapon_show_icon);
                widgets::checkbox("Show Text", &g_settings.weapon_show_text);
                widgets::slider_float("Dist. Dropoff", &g_settings.weapon_distance_dropoff, 0, 1, "%.2f");
                if (!g_settings.esp_use_theme) {
                    ImGui::ColorEdit4("Text Color##wcol", g_settings.weapon_color, ImGuiColorEditFlags_NoInputs);
                    ImGui::ColorEdit4("Icon Tint##wico", g_settings.weapon_icon_color, ImGuiColorEditFlags_NoInputs);
                }
                widgets::checkbox("Shadow##wpsh", &g_settings.weapon_shadow);
                if (g_settings.weapon_shadow)
                    ImGui::ColorEdit4("Shadow##wpsc", g_settings.weapon_shadow_color, ImGuiColorEditFlags_NoInputs);
            }
            widgets::end_panel();
        };

        auto panel_body = [&]() {
            int rows = 5;
            if (!widgets::begin_panel("Body Tuning", col_w, rows, true)) return;
            widgets::slider_float("Body Width", &g_settings.body_width_scale, 0.3f, 3.f, "%.2f");
            widgets::slider_float("Head Radius", &g_settings.head_radius, 1, 10, "%.1f");
            widgets::slider_float("Depth Scale", &g_settings.depth_scale, 100, 1500, "%.0f");
            widgets::slider_float("Glow Outer", &g_settings.glow_expand_outer, 0, 15, "%.1f");
            if (ImGui::Button("Reset Body", {-1.f, 26.f})) {
                g_settings.body_width_scale = 1.f;
                g_settings.head_radius = 6.f;
                g_settings.depth_scale = 500.f;
                g_settings.glow_expand_outer = 6.f;
                g_settings.glow_expand_inner = 3.f;
            }
            widgets::end_panel();
        };

        auto panel_radar = [&]() {
            int rows = 5 + (g_settings.radar_names ? 1 : 0);
            if (!widgets::begin_panel("Radar", col_w, rows)) return;
            widgets::checkbox("Show Radar", &g_settings.draw_radar);
            widgets::checkbox("Circle Shape", &g_settings.radar_circle);
            widgets::checkbox("Rotate with View", &g_settings.radar_rotate);
            widgets::checkbox("Range Rings", &g_settings.radar_rings);
            widgets::checkbox("Player Names", &g_settings.radar_names);
            if (g_settings.radar_names)
                widgets::slider_float("Name Font", &g_settings.radar_names_font_size, 8, 20, "%.0f");
            ImGui::ColorEdit4("Enemy##rade", g_settings.radar_enemy_color, ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4("Team##radt", g_settings.radar_team_color, ImGuiColorEditFlags_NoInputs);
            widgets::end_panel();
        };

        ImGui::BeginGroup();
        panel_esp_theme();
        ImGui::Dummy({0.f, layout::panel_gap});
        panel_esp();
        ImGui::Dummy({0.f, layout::panel_gap});
        panel_opacity();
        ImGui::Dummy({0.f, layout::panel_gap});
        panel_chams();
        ImGui::Dummy({0.f, layout::panel_gap});
        panel_box();
        ImGui::Dummy({0.f, layout::panel_gap});
        panel_health();
        ImGui::Dummy({0.f, layout::panel_gap});
        panel_name();
        ImGui::Dummy({0.f, layout::panel_gap});
        panel_weapon();
        ImGui::EndGroup();

        ImGui::SameLine(0.f, layout::column_gap);

        ImGui::BeginGroup();
        panel_body();
        ImGui::Dummy({0.f, layout::panel_gap});
        panel_radar();
        ImGui::EndGroup();
    }

    // ── Misc ───────────────────────────────────────────────────────

    void render_misc(float col_w) {
        auto panel_spectators = [&]() {
            int rows = 1 + (g_settings.draw_spectators ? 2 : 0);
            if (!widgets::begin_panel("Spectator List", col_w, rows)) return;
            widgets::checkbox("Show Spectators", &g_settings.draw_spectators);
            if (g_settings.draw_spectators) {
                widgets::slider_float("Position X", &g_settings.spec_x, -1, 3000, "%.0f");
                widgets::slider_float("Position Y", &g_settings.spec_y, 0, 2000, "%.0f");
                if (ImGui::Button("Auto##specauto", {-1.f, 26.f}))
                    g_settings.spec_x = -1.f;
            }
            widgets::end_panel();
        };

        auto panel_crosshair = [&]() {
            bool has_gap = g_settings.crosshair_shape <= 1 || g_settings.crosshair_shape == 4;
            bool not_dot = g_settings.crosshair_shape != 3;
            int rows = 1 + (g_settings.crosshair_enabled ? (5 + (has_gap ? 1 : 0) + (g_settings.crosshair_outline ? 2 : 0) + (not_dot && g_settings.crosshair_dot ? 1 : 0)) : 0);
            if (!widgets::begin_panel("Crosshair", col_w, rows)) return;
            widgets::checkbox("Enabled##xhair", &g_settings.crosshair_enabled);
            if (g_settings.crosshair_enabled) {
                static const char* shapes[] = {"Cross", "T-Shape", "Circle", "Dot", "Cross+Circle"};
                widgets::combo("Shape", &g_settings.crosshair_shape, shapes, 5);
                ImGui::ColorEdit4("Color##xcol", g_settings.crosshair_color, ImGuiColorEditFlags_NoInputs);
                widgets::slider_float("Size", &g_settings.crosshair_size, 0.5f, 20.f, "%.1f");
                widgets::slider_float("Thickness", &g_settings.crosshair_thickness, 0.5f, 5.f, "%.1f");
                if (has_gap)
                    widgets::slider_float("Gap", &g_settings.crosshair_gap, -10, 10, "%.1f");
                widgets::checkbox("Outline##xol", &g_settings.crosshair_outline);
                if (g_settings.crosshair_outline) {
                    ImGui::ColorEdit4("OL Color##xolc", g_settings.crosshair_outline_color, ImGuiColorEditFlags_NoInputs);
                    widgets::slider_float("OL Width", &g_settings.crosshair_outline_thickness, 1, 3, "%.0f");
                }
                if (not_dot) {
                    widgets::checkbox("Center Dot##xdot", &g_settings.crosshair_dot);
                    if (g_settings.crosshair_dot)
                        widgets::slider_float("Dot Size", &g_settings.crosshair_dot_size, 1, 4, "%.0f");
                }
            }
            widgets::end_panel();
        };

        auto panel_nades = [&]() {
            if (!widgets::begin_panel("Grenade Helper", col_w, 7)) return;
            widgets::checkbox("Enabled##nades", &g_settings.grenade_helper_enabled);
            widgets::combo("Filter", &m_filter_idx, nade_filter_items, IM_ARRAYSIZE(nade_filter_items));
            update_nade_filter();
            ImGui::ColorEdit4("Circle##ncc", g_settings.grenade_circle_color, ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4("Active Circle##nac", g_settings.grenade_circle_active_color, ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4("Aim Line##nal", g_settings.grenade_aim_line_color, ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4("Text##ntc", g_settings.grenade_text_color, ImGuiColorEditFlags_NoInputs);
            widgets::slider_float("Circle Radius", &g_settings.grenade_circle_radius, 10, 150, "%.0f");
            widgets::slider_float("Circle Thick.", &g_settings.grenade_circle_thickness, 0.5f, 4.f, "%.1f");
            widgets::end_panel();
        };

        ImGui::BeginGroup();
        panel_spectators();
        ImGui::Dummy({0.f, layout::panel_gap});
        panel_crosshair();
        ImGui::EndGroup();

        ImGui::SameLine(0.f, layout::column_gap);

        ImGui::BeginGroup();
        panel_nades();
        ImGui::EndGroup();
    }

    void update_nade_filter() {
        g_settings.grenade_filter_smoke   = (m_filter_idx == 0 || m_filter_idx == 1);
        g_settings.grenade_filter_molotov = (m_filter_idx == 0 || m_filter_idx == 2);
        g_settings.grenade_filter_frag    = (m_filter_idx == 0 || m_filter_idx == 3);
        g_settings.grenade_filter_flash   = (m_filter_idx == 0 || m_filter_idx == 4);
    }

    // ── Settings ───────────────────────────────────────────────────

    void render_settings(float col_w) {
        auto panel_general = [&]() {
            if (!widgets::begin_panel("General", col_w, 4)) return;
            widgets::checkbox("Master Switch", &g_settings.master_switch);
            widgets::checkbox("Vsync", &g_settings.use_vsync);
            widgets::slider_float("Target FPS", &g_settings.target_fps, 30, 144, "%.0f");
            widgets::slider_float("Background Alpha", &g_settings.menu_bg_alpha, 0.3f, 1.f, "%.2f", true);
            widgets::end_panel();
        };

        auto panel_keybinds = [&]() {
            if (!widgets::begin_panel("Key Binds", col_w, 3)) return;
            render_key_bind("Menu Toggle", g_settings.key_menu, bind_waiting_menu);
            render_key_bind("Master Toggle", g_settings.key_master, bind_waiting_master);
            render_key_bind("Exit", g_settings.key_exit, bind_waiting_exit, true);
            widgets::end_panel();
        };

        auto panel_config = [&]() {
            if (!widgets::begin_panel("Config", col_w, 1)) return;
            if (ImGui::Button("Reset All Settings", {-1.f, 26.f})) {
                reset_popup_open = true;
            }
            if (reset_popup_open) {
                ImGui::OpenPopup("Reset?");
                reset_popup_open = false;
            }
            if (ImGui::BeginPopupModal("Reset?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImFont* f = fonts::regular();
                if (f) {
                    ImGui::PushFont(f);
                    ImGui::Text("Reset ALL settings to defaults?");
                    ImGui::PopFont();
                }
                if (ImGui::Button("Yes", {100.f, 0.f})) {
                    g_settings.reset();
                    g_overlay.font_rebuild_needed = true;
                    g_overlay.apply_menu_style();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", {100.f, 0.f}))
                    ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
            widgets::end_panel();
        };

        ImGui::BeginGroup();
        panel_general();
        ImGui::Dummy({0.f, layout::panel_gap});
        panel_keybinds();
        ImGui::EndGroup();

        ImGui::SameLine(0.f, layout::column_gap);

        ImGui::BeginGroup();
        panel_config();
        ImGui::EndGroup();
    }

    // ── Key bind helper ────────────────────────────────────────────

    bool bind_waiting_menu = false;
    bool bind_waiting_master = false;
    bool bind_waiting_exit = false;

    void render_key_bind(const char* label, int& key, bool& waiting, bool last_element = false) {
        ImGui::Text("%s", label);
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 120);

        char btn[64];
        if (waiting)
            snprintf(btn, sizeof(btn), "[...]##%s", label);
        else
            snprintf(btn, sizeof(btn), "%s##%s", vk_name(key), label);

        if (ImGui::Button(btn, {100.f, 22.f}))
            waiting = true;

        if (waiting) {
            int pressed = scan_any_key(false);
            if (pressed > 0)   { key = pressed; waiting = false; }
            else if (pressed == -1) { waiting = false; }
        }
    }
};

inline Menu g_menu;
