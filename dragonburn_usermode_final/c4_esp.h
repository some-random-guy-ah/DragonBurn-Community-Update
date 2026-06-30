#pragma once
#include <imgui.h>
#include <string>
#include <cstdio>
#include "types.h"
#include "settings.h"
#include "offsets.h"
#include "memory/imemory.h"
#include "overlay.h"

struct PlantedC4State {
    uintptr_t entity = 0;
    Vec3  world_pos{};
    int   bomb_site = 0;
    bool  being_defused = false;
    float defuse_end_time = 0.f;
    float c4_blow_time = 0.f;
    bool  valid = false;
};

static float get_cur_time() {
    if (!g_offsets.client.dwGlobalVars) return 0.f;
    uintptr_t gv = g_memory->read<uintptr_t>(
        g_memory->get_client_base() + g_offsets.client.dwGlobalVars);
    if (!gv) return 0.f;
    return g_memory->read<float>(gv + 0x30);
}

static PlantedC4State read_planted_c4()
{
    PlantedC4State state{};
    uintptr_t client = g_memory->get_client_base();
    if (!client || !g_offsets.client.dwPlantedC4) return state;

    bool is_bomb_planted = false;
    g_memory->read_raw(client + g_offsets.client.dwPlantedC4 - 0x8, &is_bomb_planted, 1);
    if (!is_bomb_planted) return state;

    uintptr_t planted_ptr = g_memory->read<uintptr_t>(client + g_offsets.client.dwPlantedC4);
    if (!planted_ptr) return state;

    uintptr_t c4_entity = g_memory->read<uintptr_t>(planted_ptr);
    if (!c4_entity) return state;

    uintptr_t scene = g_memory->read<uintptr_t>(c4_entity + g_offsets.C_BaseEntity.m_pGameSceneNode);
    if (!scene) return state;

    state.world_pos = g_memory->read<Vec3>(scene + g_offsets.CGameSceneNode.m_vecAbsOrigin);

    if (g_offsets.C4.m_nBombSite)
        state.bomb_site = g_memory->read<int>(c4_entity + g_offsets.C4.m_nBombSite);
    if (g_offsets.C4.m_bBeingDefused)
        state.being_defused = g_memory->read<bool>(c4_entity + g_offsets.C4.m_bBeingDefused);
    if (g_offsets.C4.m_flDefuseCountDown)
        state.defuse_end_time = g_memory->read<float>(c4_entity + g_offsets.C4.m_flDefuseCountDown);
    if (g_offsets.C4.m_flC4Blow)
        state.c4_blow_time = g_memory->read<float>(c4_entity + g_offsets.C4.m_flC4Blow);

    state.entity = c4_entity;
    state.valid = true;
    return state;
}

static void draw_c4_esp(ImDrawList* draw, const PlantedC4State& c4, int sw, int sh, const Matrix4x4& vm)
{
    if (!g_settings.c4_esp_enabled || !g_settings.master_switch) return;

    ImVec2 screen;
    if (!w2s(c4.world_pos, vm, sw, sh, screen))
        return;

    ImU32 text_color = ImGui::GetColorU32({1.0f, 0.47f, 0.47f, 1.0f});
    ImU32 defuse_color = ImGui::GetColorU32({1.0f, 0.84f, 0.47f, 1.0f});

    ImFont* font = g_overlay.esp_font;
    float font_size = g_settings.esp_font_atlas_size;

    if (font) {
        ImU32 shadow = IM_COL32(0, 0, 0, 230);
        auto draw_shadowed = [&](float fs, const char* txt, ImVec2 pos, ImU32 col) {
            ImVec2 ts = font->CalcTextSizeA(fs, FLT_MAX, 0.f, txt);
            ImVec2 p(pos.x - ts.x * 0.5f, pos.y);
            draw->AddText(font, fs, {p.x - 1, p.y - 1}, shadow, txt);
            draw->AddText(font, fs, {p.x + 1, p.y - 1}, shadow, txt);
            draw->AddText(font, fs, {p.x - 1, p.y + 1}, shadow, txt);
            draw->AddText(font, fs, {p.x + 1, p.y + 1}, shadow, txt);
            draw->AddText(font, fs, p, col, txt);
        };

        draw_shadowed(font_size, "PLANTED C4", {screen.x, screen.y - font_size - 4.f}, text_color);

        char site_label[32]; snprintf(site_label, sizeof(site_label), "[%s]", (c4.bomb_site == 0) ? "A" : "B");
        draw_shadowed(font_size, site_label, {screen.x, screen.y + 2.f}, text_color);

        if (c4.being_defused) {
            float remaining = c4.defuse_end_time - get_cur_time();
            if (remaining < 0.f || remaining > 10.f) remaining = 0.f;
            char buf[64]; snprintf(buf, sizeof(buf), "DEFUSING (%.1fs)", remaining);
            draw_shadowed(font_size, buf, {screen.x, screen.y + font_size + 6.f}, defuse_color);
        }
    }
}

static void draw_bomb_timer(const PlantedC4State& c4)
{
    if (!g_settings.show_bomb_timer || !c4.valid || !g_settings.master_switch)
        return;

    float cur_time = 0.f;
    if (g_offsets.client.dwGlobalVars) {
        uintptr_t gv = g_memory->read<uintptr_t>(
            g_memory->get_client_base() + g_offsets.client.dwGlobalVars);
        if (gv) cur_time = g_memory->read<float>(gv + 0x30);
    }
    float remaining = c4.c4_blow_time - cur_time;
    if (remaining < 0.f) remaining = 0.f;
    if (remaining > 40.f) remaining = 40.f;

    float progress = remaining / 40.f;

    ImGui::SetNextWindowBgAlpha(0.85f);
    ImGui::SetNextWindowSize({260.f, 90.f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos({10.f, 200.f}, ImGuiCond_FirstUseEver);

    ImGui::Begin("Bomb Timer", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_AlwaysAutoResize);

    const char* site = (c4.bomb_site == 0) ? "A" : "B";
    ImGui::Text("Bomb on %s", site);
    ImGui::Dummy({0.f, 4.f});

    char timer_buf[32]; snprintf(timer_buf, sizeof(timer_buf), "%.2f", remaining);
    ImVec2 timer_size = ImGui::CalcTextSize(timer_buf);
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - timer_size.x) * 0.5f);
    ImGui::Text("%s", timer_buf);
    ImGui::Dummy({0.f, 2.f});

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    float bar_h = 12.f;

    ImU32 bg_col = ImGui::GetColorU32({0.15f, 0.15f, 0.15f, 0.8f});
    ImU32 fill_col;
    if (c4.being_defused && remaining > 5.f)
        fill_col = ImGui::GetColorU32({0.125f, 0.70f, 0.67f, 1.0f});
    else if (remaining <= 10.f)
        fill_col = ImGui::GetColorU32({0.63f, 0.19f, 0.29f, 1.0f});
    else
        fill_col = ImGui::GetColorU32({0.51f, 0.54f, 0.59f, 1.0f});

    dl->AddRectFilled(pos, {pos.x + w, pos.y + bar_h}, bg_col, 4.f);
    if (progress > 0.f)
        dl->AddRectFilled(pos, {pos.x + w * progress, pos.y + bar_h}, fill_col, 4.f);

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + bar_h + 4.f);

    if (c4.being_defused) {
        float defuse_remaining = c4.defuse_end_time - get_cur_time();
        if (defuse_remaining < 0.f || defuse_remaining > 10.f) defuse_remaining = 0.f;
        char def_buf[64]; snprintf(def_buf, sizeof(def_buf), "Defusing: %.2f s", defuse_remaining);
        ImVec2 def_size = ImGui::CalcTextSize(def_buf);
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - def_size.x) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(0.125f, 0.70f, 0.67f, 1.0f));
        ImGui::Text("%s", def_buf);
        ImGui::PopStyleColor();
    }
    ImGui::End();
}

// -----------------------------------------------------------------------
// Dropped C4 (on ground, not planted)
// -----------------------------------------------------------------------
struct DroppedC4State {
    uintptr_t entity = 0;
    Vec3  world_pos{};
    bool  valid = false;
};

static DroppedC4State read_dropped_c4()
{
    DroppedC4State state{};
    uintptr_t client = g_memory->get_client_base();
    if (!client || !g_offsets.client.dwWeaponC4) return state;

    // Don't show dropped C4 while C4 is planted
    if (g_offsets.client.dwPlantedC4) {
        bool is_bomb_planted = false;
        g_memory->read_raw(client + g_offsets.client.dwPlantedC4 - 0x8, &is_bomb_planted, 1);
        if (is_bomb_planted) return state;
    }

    uintptr_t c4_ptr = g_memory->read<uintptr_t>(client + g_offsets.client.dwWeaponC4);
    if (!c4_ptr) return state;

    uintptr_t c4_entity = g_memory->read<uintptr_t>(c4_ptr);
    if (!c4_entity) return state;

    // Verify it's still a C4 weapon
    uint16_t def_index = g_memory->read<uint16_t>(
        c4_entity + g_offsets.C_EconEntity.m_AttributeManager
               + g_offsets.C_AttributeContainer.m_Item
               + g_offsets.C_EconItemView.m_iItemDefinitionIndex);
    if (def_index != 49) return state;

    uintptr_t scene = g_memory->read<uintptr_t>(c4_entity + g_offsets.C_BaseEntity.m_pGameSceneNode);
    if (!scene) return state;

    state.world_pos = g_memory->read<Vec3>(scene + g_offsets.CGameSceneNode.m_vecAbsOrigin);
    if (state.world_pos.x == 0.f && state.world_pos.y == 0.f && state.world_pos.z == 0.f)
        return state;

    state.entity = c4_entity;
    state.valid = true;
    return state;
}

static void draw_dropped_c4_esp(ImDrawList* draw, const DroppedC4State& c4, int sw, int sh, const Matrix4x4& vm)
{
    if (!g_settings.c4_esp_enabled || !g_settings.master_switch) return;

    ImVec2 screen;
    if (!w2s(c4.world_pos, vm, sw, sh, screen))
        return;

    ImFont* font = g_overlay.esp_font;
    float font_size = g_settings.esp_font_atlas_size;

    if (font) {
        ImU32 col = IM_COL32(255, 165, 0, 230);
        ImU32 shadow = IM_COL32(0, 0, 0, 230);
        ImVec2 ts = font->CalcTextSizeA(font_size, FLT_MAX, 0.f, "DROPPED C4");
        float lx = screen.x - ts.x * 0.5f;
        float ly = screen.y - ts.y - 4.f;
        draw->AddText(font, font_size, {lx - 1, ly - 1}, shadow, "DROPPED C4");
        draw->AddText(font, font_size, {lx + 1, ly - 1}, shadow, "DROPPED C4");
        draw->AddText(font, font_size, {lx - 1, ly + 1}, shadow, "DROPPED C4");
        draw->AddText(font, font_size, {lx + 1, ly + 1}, shadow, "DROPPED C4");
        draw->AddText(font, font_size, {lx, ly}, col, "DROPPED C4");
    }
}
