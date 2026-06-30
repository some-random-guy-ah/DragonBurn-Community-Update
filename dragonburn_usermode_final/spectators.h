#pragma once
#include <imgui.h>
#include <vector>
#include <string>
#include <cstdint>
#include "memory/memory_syscall.h"
#include "offsets.h"
#include "settings.h"
#include "entity_utils.h"
#include "overlay.h"

class SpectatorList {
public:
    std::vector<std::string> spectators;

    void update(uintptr_t entity_list, uintptr_t local_pawn, uintptr_t local_controller) {
        spectators.clear();
        if (!entity_list || !local_pawn) return;

        uint32_t local_handle_player = 0;
        uint32_t local_handle_pawn = 0;
        if (local_controller) {
            local_handle_player = g_memory->read<uint32_t>(
                local_controller + g_offsets.CCSPlayerController.m_hPlayerPawn);
            local_handle_pawn = g_memory->read<uint32_t>(
                local_controller + g_offsets.CCSPlayerController.m_hPawn);
        }

        uintptr_t first_page = g_memory->read<uintptr_t>(
            entity_list + EntityList::PAGE_HEADER);
        if (!first_page) return;

        for (int i = 1; i < EntityList::MAX_PLAYERS; i++) {
            uintptr_t controller = g_memory->read<uintptr_t>(
                first_page + EntityList::ENTRY_STRIDE * (i & EntityList::INDEX_MASK));
            if (!controller || controller == local_controller) continue;

            char name[128]{};
            read_player_name(controller, name, sizeof(name));
            if (!name[0]) continue;

            uint32_t pawn_handle = get_pawn_handle(controller);
            if (!pawn_handle) continue;

            uintptr_t pawn = EntityList::resolve_handle(entity_list, pawn_handle);
            if (!pawn || pawn == local_pawn) continue;

            int health = g_memory->read<int>(pawn + g_offsets.C_BaseEntity.m_iHealth);
            int team = g_memory->read<int>(pawn + g_offsets.C_BaseEntity.m_iTeamNum);
            if (health > 0 && team != 1) continue;

            uintptr_t obs_svc = g_memory->read<uintptr_t>(
                pawn + g_offsets.C_BasePlayerPawn.m_pObserverServices);
            if (!obs_svc) continue;

            uint32_t obs_target = g_memory->read<uint32_t>(
                obs_svc + g_offsets.CPlayer_ObserverServices.m_hObserverTarget);
            if (!obs_target || obs_target == 0xFFFFFFFF) continue;

            bool match = false;
            if (local_handle_pawn && obs_target == local_handle_pawn) match = true;
            if (!match && local_handle_player && obs_target == local_handle_player) match = true;
            if (!match) {
                uintptr_t resolved = EntityList::resolve_handle(entity_list, obs_target);
                if (resolved == local_pawn) match = true;
            }

            if (match) {
                bool dup = false;
                for (const auto& s : spectators)
                    if (s == name) { dup = true; break; }
                if (!dup) spectators.push_back(name);
            }
        }
    }

    void draw(int screen_w) {
        if (!g_settings.draw_spectators || !g_settings.master_switch) return;

        char title[64];
        snprintf(title, sizeof(title), "Spectators (%d)###spec", (int)spectators.size());

        float window_w = 280.0f;

        // Use configurable position, default to right side
        float sx = g_settings.spec_x;
        float sy = g_settings.spec_y;
        if (sx < 0) sx = (float)screen_w - window_w - 10.0f;

        ImGui::SetNextWindowPos({sx, sy}, ImGuiCond_Always);
        ImGui::SetNextWindowSizeConstraints({window_w, 0}, {window_w, 1000});
        ImGui::SetNextWindowBgAlpha(0.7f);

        int flags = ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_AlwaysAutoResize |
                    ImGuiWindowFlags_NoFocusOnAppearing |
                    ImGuiWindowFlags_NoNav |
                    ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoInputs;

        ImFont* font = g_overlay.spec_font;
        if (font) ImGui::PushFont(font);

        ImGui::Begin(title, nullptr, flags);

        for (const auto& s : spectators)
            ImGui::TextColored({1.0f, 0.4f, 0.4f, 1}, "> %s", s.c_str());

        ImGui::End();

        if (font) ImGui::PopFont();
    }
};

inline SpectatorList g_spectators;