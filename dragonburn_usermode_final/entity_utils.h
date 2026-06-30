#pragma once
#include <cstdint>
#include "offsets.h"

namespace EntityList {
    constexpr size_t ENTRY_STRIDE = 112;     // 0x70
    constexpr size_t PAGE_HEADER = 16;       // 0x10
    constexpr size_t PAGE_PTR_SIZE = 8;
    constexpr uint32_t HANDLE_MASK = 0x7FFF;
    constexpr uint32_t PAGE_SHIFT = 9;
    constexpr uint32_t INDEX_MASK = 0x1FF;
    constexpr int MAX_PLAYERS = 64;

    inline uintptr_t get_page(uintptr_t list, uint32_t handle) {
        return g_memory->read<uintptr_t>(
            list + PAGE_PTR_SIZE * ((handle & HANDLE_MASK) >> PAGE_SHIFT) + PAGE_HEADER);
    }

    inline uintptr_t get_entry(uintptr_t page, uint32_t handle) {
        return g_memory->read<uintptr_t>(page + ENTRY_STRIDE * (handle & INDEX_MASK));
    }

    inline uintptr_t resolve_handle(uintptr_t list, uint32_t handle) {
        if (!handle) return 0;
        uintptr_t page = get_page(list, handle);
        if (!page) return 0;
        return get_entry(page, handle);
    }
}

inline uint32_t get_pawn_handle(uintptr_t controller) {
    uint32_t h = g_memory->read<uint32_t>(
        controller + g_offsets.CCSPlayerController.m_hPawn);
    if (!h)
        h = g_memory->read<uint32_t>(
            controller + g_offsets.CCSPlayerController.m_hPlayerPawn);
    return h;
}

inline void read_player_name(uintptr_t controller, char* out, size_t max_len) {
    out[0] = 0;
    uintptr_t ptr = g_memory->read<uintptr_t>(
        controller + g_offsets.CCSPlayerController.m_sSanitizedPlayerName);
    if (!ptr) return;
    g_memory->read_raw(ptr, out, max_len - 1);
    out[max_len - 1] = 0;
    for (size_t i = 0; out[i]; i++)
        if ((unsigned char)out[i] < 0x20) out[i] = ' ';
}