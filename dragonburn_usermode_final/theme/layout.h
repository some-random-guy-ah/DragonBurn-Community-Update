#pragma once

namespace layout
{
    inline constexpr float window_w = 740.f;
    inline constexpr float window_h = 520.f;
    inline constexpr float round = 3.f;
    inline constexpr float shell_round = round;
    inline constexpr float sidebar_w = 0.f;
    inline constexpr float content_pad = 10.f;
    inline constexpr float combat_tabs_h = 34.f;
    inline constexpr float header_gap = 2.f;
    inline constexpr float row_widget_y_nudge = 0.f;
    inline constexpr float search_max_w = 220.f;
    inline constexpr float column_gap = 8.f;
    inline constexpr float panel_gap = 8.f;
    inline constexpr float panel_round = round;
    inline constexpr float panel_header_h = 34.f;
    inline constexpr float panel_pad_x = 10.f;
    inline constexpr float panel_pad_y = 8.f;
    inline constexpr float panel_body_slack = 4.f;
    inline constexpr float row_h = 30.f;
    inline constexpr float slider_row_h = 30.f;
    inline constexpr float row_right_pad = 0.f;
    inline constexpr float checkbox_size = 18.f;
    inline constexpr float checkbox_round = round;
    inline constexpr float checkbox_text_gap = 8.f;
    inline constexpr float accent_bar_w = 3.f;
    inline constexpr float accent_bar_x = 4.f;
    inline constexpr float slider_fill_h = 6.f;
    inline constexpr float slider_track_h = 2.f;
    inline constexpr float slider_label_gap = 8.f;
    inline constexpr float slider_value_gap = 6.f;
    inline constexpr float slider_min_w = 72.f;
    inline constexpr float cog_w = 16.f;
    inline constexpr float cog_gap = 6.f;
    inline constexpr float combo_h = 24.f;
    inline constexpr float combo_round = round;
    inline constexpr float combo_max_w = 168.f;
    inline constexpr float dropdown_item_h = 28.f;
    inline constexpr float content_search_h = 30.f;
    inline constexpr float keybind_h = 24.f;
    inline constexpr float keybind_w = 120.f;
    inline constexpr float keybind_pad = 3.f;
    inline constexpr float keybind_icon_box = 22.f;
    inline constexpr float keybind_icon_size = 13.f;
    inline constexpr float keybind_round = round;
    inline constexpr float keybind_icon_round = round;
    inline constexpr float keybind_dropdown_w = 132.f;

    inline constexpr const char* main_window_id = "##imgui_portfolio_5";
    inline constexpr float external_popup_gap = 10.f;
    inline constexpr float external_popup_w = 200.f;

    inline constexpr float sidebar_accent_x = 3.f;
    inline constexpr float sidebar_round = 0.f;

    inline float panel_body_height(int rows, bool slider = false, int keybind_rows = 0)
    {
        float h = panel_pad_y * 2.f + (float)rows * row_h + panel_body_slack;
        if (slider)
            h += slider_row_h - row_h;
        if (keybind_rows > 0)
            h += (float)keybind_rows * (keybind_h - row_h);
        return h;
    }

    inline float panel_height(int rows, bool slider = false, int keybind_rows = 0)
    {
        return panel_header_h + panel_body_height(rows, slider, keybind_rows);
    }
}
