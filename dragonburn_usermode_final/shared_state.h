#pragma once

#include <atomic>
#include <mutex>
#include "types.h"

// Minimal struct with only what aimbot needs
struct AimbotFrame {
    Matrix4x4 view_matrix{};
    float local_x = 0, local_y = 0, local_z = 0;
    int local_team = 0;
    uintptr_t local_pawn = 0;
    int screen_w = 0;
    int screen_h = 0;

    Vec3  eye_origin{};
    Vec3  view_angles{};   // pitch, yaw, roll
    float camera_fov = 90.0f;
    bool  camera_valid = false;
    bool  is_scoped = false;

    struct Target {
        bool valid = false;
        int team = 0;
        int health = 0;
        Vec3 head_pos{};
        Vec3 neck_pos{};
        Vec3 chest_pos{};
        Vec3 pelvis_pos{};
    };

    Target targets[64]{};
};

// Double-buffer: main loop writes, aimbot reads, no blocking either side
class SharedAimbotData {
public:
    void publish(const AimbotFrame& frame) {
        int write_idx = m_read_index.load(std::memory_order_relaxed) ^ 1;
        m_frames[write_idx] = frame;
        m_read_index.store(write_idx, std::memory_order_release);
    }

    AimbotFrame snapshot() const {
        int idx = m_read_index.load(std::memory_order_acquire);
        return m_frames[idx];
    }

private:
    AimbotFrame m_frames[2]{};
    std::atomic<int> m_read_index{0};
};

inline SharedAimbotData g_aimbot_data;