#pragma once

#include <map>
#include <string>

#include "imgui.h"
#include "imgui_internal.h"

struct WidgetAnim
{
    float active = 0.f;
    float hover = 0.f;
    float slide = -1.f;
    float open = 1.f;
    float pulse = 0.f;
};

namespace widgets
{
    extern std::map<ImGuiID, WidgetAnim> g_anims;

    bool begin_panel(const char* title, float width, int rows, bool has_slider = false, bool default_open = true, int keybind_rows = 0);
    void end_panel();

    bool checkbox(const char* label, bool* value, bool last_element = false, float right_inset = 0.f);
    bool checkbox_keybind_row(const char* label, bool* value, const char* id, int* key, int* mode, bool last_element = false);
    bool slider_float(const char* label, float* value, float min, float max, const char* format = "%.0f", bool last_element = false);
    bool combo(const char* label, int* index, const char* const items[], int count, bool last_element = false);
    bool settings_cog(const char* id, int slot = 0, bool active = false);
    void end_settings_popup();

    bool keybind_setter(const char* id, int* key, int* mode);

    bool selectable_item(const char* label, bool selected);
    bool combo_selectable_item(const char* label, bool selected);
}
