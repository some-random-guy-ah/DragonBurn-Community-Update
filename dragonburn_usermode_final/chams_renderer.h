#pragma once
#include <imgui.h>
#include <cmath>
#include <algorithm>
#include "types.h"
#include "settings.h"
#include "utils.h"

// Styles: 0=Filled, 1=Wireframe, 2=Glow, 3=Skeleton
enum class ChamsStyle { FILLED, WIREFRAME, GLOW, SKELETON };

struct ColorSet {
    ImU32 fill, outline, glow, wire, head_fill;
};

inline ColorSet get_enemy_colors(float opacity = 1.0f) {
    ImU32 fill    = apply_opacity(float4_to_col(g_settings.enemy_fill),    opacity);
    ImU32 outline = apply_opacity(float4_to_col(g_settings.enemy_outline), opacity);
    ImU32 glow    = apply_opacity(float4_to_col(g_settings.enemy_glow),    opacity);
    return {
        fill, outline, glow, outline,
        apply_opacity((fill & 0x00FFFFFF) | 0x60000000, opacity),
    };
}

inline ColorSet get_team_colors(float opacity = 1.0f) {
    ImU32 fill    = apply_opacity(float4_to_col(g_settings.team_fill),    opacity);
    ImU32 outline = apply_opacity(float4_to_col(g_settings.team_outline), opacity);
    ImU32 glow    = apply_opacity(float4_to_col(g_settings.team_glow),    opacity);
    return {
        fill, outline, glow, outline,
        apply_opacity((fill & 0x00FFFFFF) | 0x60000000, opacity),
    };
}

struct LimbQuad {
    ImVec2 p[4];
    bool valid;
};

class ChamsRenderer {
public:
    static LimbQuad build_limb_quad(const PlayerVisuals& p, const LimbDef& limb,
                                    float depth_scale, float extra = 0) {
        LimbQuad q{};
        q.valid = false;
        if (!p.visible[limb.bone_a] || !p.visible[limb.bone_b]) return q;

        ImVec2 a = p.screens[limb.bone_a], b = p.screens[limb.bone_b];
        float da = p.depths[limb.bone_a], db = p.depths[limb.bone_b];

        float sc = g_settings.body_width_scale;
        float wa = (limb.width_a * sc + extra) * depth_scale / da;
        float wb = (limb.width_b * sc + extra) * depth_scale / db;

        float dx = b.x - a.x, dy = b.y - a.y;
        float len = sqrtf(dx * dx + dy * dy);
        if (len < 0.5f) return q;

        float nx = -dy / len, ny = dx / len;
        q.p[0] = {a.x + nx * wa, a.y + ny * wa};
        q.p[1] = {a.x - nx * wa, a.y - ny * wa};
        q.p[2] = {b.x - nx * wb, b.y - ny * wb};
        q.p[3] = {b.x + nx * wb, b.y + ny * wb};
        q.valid = true;
        return q;
    }

    static void draw_ellipse_filled(ImDrawList* d, ImVec2 center,
                                    float rx, float ry, float angle,
                                    ImU32 col, int segments = 20) {
        if (rx < 0.5f || ry < 0.5f) return;
        float cos_a = cosf(angle), sin_a = sinf(angle);
        ImVec2 prev;
        for (int i = 0; i <= segments; i++) {
            float t = (float)i / segments * 6.28318530f;
            float ex = rx * cosf(t), ey = ry * sinf(t);
            ImVec2 pt = {
                center.x + ex * cos_a - ey * sin_a,
                center.y + ex * sin_a + ey * cos_a
            };
            if (i > 0) d->AddTriangleFilled(center, prev, pt, col);
            prev = pt;
        }
    }

    static void draw_ellipse_stroke(ImDrawList* d, ImVec2 center,
                                    float rx, float ry, float angle,
                                    ImU32 col, float thickness = 1.0f,
                                    int segments = 20) {
        if (rx < 0.5f || ry < 0.5f) return;
        float cos_a = cosf(angle), sin_a = sinf(angle);
        ImVec2 prev;
        for (int i = 0; i <= segments; i++) {
            float t = (float)i / segments * 6.28318530f;
            float ex = rx * cosf(t), ey = ry * sinf(t);
            ImVec2 pt = {
                center.x + ex * cos_a - ey * sin_a,
                center.y + ex * sin_a + ey * cos_a
            };
            if (i > 0) d->AddLine(prev, pt, col, thickness);
            prev = pt;
        }
    }

    // ===== BODY STYLES =====

    static void draw_body_filled(ImDrawList* d, const PlayerVisuals& p,
                                 const ColorSet& c, bool shade, float depth_scale) {
        float sc = g_settings.body_width_scale;
        for (int i = 0; i < BODY_LIMB_COUNT; i++) {
            const auto& limb = BODY_LIMBS[i];
            if (!p.visible[limb.bone_a] || !p.visible[limb.bone_b]) continue;
            ImVec2 a = p.screens[limb.bone_a], b = p.screens[limb.bone_b];
            float da = p.depths[limb.bone_a], db = p.depths[limb.bone_b];
            float dx = b.x - a.x, dy = b.y - a.y;
            float len = sqrtf(dx * dx + dy * dy);
            if (len < 1.0f) continue;
            float angle = atan2f(dy, dx);
            int slices = std::clamp((int)(len / 6.0f), 3, 12);
            for (int s = 0; s <= slices; s++) {
                float t = (float)s / slices;
                ImVec2 pos = {a.x + dx * t, a.y + dy * t};
                float depth = da + (db - da) * t;
                float w = (limb.width_a + (limb.width_b - limb.width_a) * t) * sc;
                float rx = w * depth_scale / depth;
                float ry = rx * 0.5f;
                ImU32 base = shade ? darken(c.fill, 0.7f + 0.3f * t) : c.fill;
                draw_ellipse_filled(d, pos, rx, ry, angle, slice_color(base, slices + 1), 14);
            }
        }
    }

    static void draw_body_outline(ImDrawList* d, const PlayerVisuals& p,
                                  const ColorSet& c, float depth_scale) {
        float sc = g_settings.body_width_scale;
        for (int i = 0; i < BODY_LIMB_COUNT; i++) {
            const auto& limb = BODY_LIMBS[i];
            if (!p.visible[limb.bone_a] || !p.visible[limb.bone_b]) continue;
            ImVec2 a = p.screens[limb.bone_a], b = p.screens[limb.bone_b];
            float da = p.depths[limb.bone_a], db = p.depths[limb.bone_b];
            float dx = b.x - a.x, dy = b.y - a.y;
            float len = sqrtf(dx * dx + dy * dy);
            if (len < 1.0f) continue;
            float angle = atan2f(dy, dx);
            float wa = limb.width_a * sc * depth_scale / da;
            float wb = limb.width_b * sc * depth_scale / db;
            float nx = -dy / len, ny = dx / len;
            d->AddLine({a.x + nx * wa, a.y + ny * wa},
                       {b.x + nx * wb, b.y + ny * wb}, c.outline, 1.2f);
            d->AddLine({a.x - nx * wa, a.y - ny * wa},
                       {b.x - nx * wb, b.y - ny * wb}, c.outline, 1.2f);
            draw_ellipse_stroke(d, a, wa, wa * 0.5f, angle, c.outline, 0.8f, 12);
            draw_ellipse_stroke(d, b, wb, wb * 0.5f, angle, c.outline, 0.8f, 12);
        }
    }

    static void draw_body_wireframe(ImDrawList* d, const PlayerVisuals& p,
                                    const ColorSet& c, float depth_scale) {
        float sc = g_settings.body_width_scale;
        for (int i = 0; i < BODY_LIMB_COUNT; i++) {
            const auto& limb = BODY_LIMBS[i];
            if (!p.visible[limb.bone_a] || !p.visible[limb.bone_b]) continue;
            ImVec2 a = p.screens[limb.bone_a], b = p.screens[limb.bone_b];
            float da = p.depths[limb.bone_a], db = p.depths[limb.bone_b];
            float wa = limb.width_a * sc * depth_scale / da;
            float wb = limb.width_b * sc * depth_scale / db;
            float dx = b.x - a.x, dy = b.y - a.y;
            float len = sqrtf(dx * dx + dy * dy);
            if (len < 1.0f) continue;
            float nx = -dy / len, ny = dx / len;
            ImVec2 p0 = {a.x + nx * wa, a.y + ny * wa};
            ImVec2 p1 = {a.x - nx * wa, a.y - ny * wa};
            ImVec2 p2 = {b.x - nx * wb, b.y - ny * wb};
            ImVec2 p3 = {b.x + nx * wb, b.y + ny * wb};
            d->AddQuadFilled(p0, p1, p2, p3, (c.fill & 0x00FFFFFF) | 0x10000000);
            d->AddLine(p0, p3, c.wire, 1.3f);
            d->AddLine(p1, p2, c.wire, 1.3f);
            d->AddLine(p0, p1, c.wire, 1.0f);
            d->AddLine(p2, p3, c.wire, 1.0f);
            d->AddLine(p0, p2, c.wire, 0.5f);
            d->AddLine(p1, p3, c.wire, 0.5f);
            if (len > 20.0f) {
                float mw = (wa + wb) * 0.5f;
                ImVec2 mid_a = {(a.x + b.x) * 0.5f + nx * mw, (a.y + b.y) * 0.5f + ny * mw};
                ImVec2 mid_b = {(a.x + b.x) * 0.5f - nx * mw, (a.y + b.y) * 0.5f - ny * mw};
                d->AddLine(mid_a, mid_b, c.wire, 0.6f);
            }
        }
    }

    static void draw_body_glow(ImDrawList* d, const PlayerVisuals& p,
                               const ColorSet& c, float depth_scale, float expand) {
        float sc = g_settings.body_width_scale;
        for (int i = 0; i < BODY_LIMB_COUNT; i++) {
            const auto& limb = BODY_LIMBS[i];
            if (!p.visible[limb.bone_a] || !p.visible[limb.bone_b]) continue;
            ImVec2 a = p.screens[limb.bone_a], b = p.screens[limb.bone_b];
            float da = p.depths[limb.bone_a], db = p.depths[limb.bone_b];
            float dx = b.x - a.x, dy = b.y - a.y;
            float len = sqrtf(dx * dx + dy * dy);
            if (len < 1.0f) continue;
            float angle = atan2f(dy, dx);
            int slices = std::clamp((int)(len / 8.0f), 2, 8);
            for (int s = 0; s <= slices; s++) {
                float t = (float)s / slices;
                ImVec2 pos = {a.x + dx * t, a.y + dy * t};
                float depth = da + (db - da) * t;
                float w = (limb.width_a + (limb.width_b - limb.width_a) * t) * sc;
                float rx = (w + expand) * depth_scale / depth;
                float ry = rx * 0.5f;
                draw_ellipse_filled(d, pos, rx, ry, angle, slice_color(c.glow, slices + 1), 12);
            }
        }
    }

    // ===== HEAD =====

    static void draw_head(ImDrawList* d, const PlayerVisuals& p,
                          const ColorSet& c, bool glow, float depth_scale) {
        if (!p.visible[BONE_HEAD] || !p.visible[BONE_NECK]) return;
        float r = std::clamp(g_settings.head_radius * depth_scale / p.depths[BONE_HEAD],
                             2.0f, 80.0f);
        ImVec2 head = p.screens[BONE_HEAD];
        ImVec2 neck = p.screens[BONE_NECK];

        float neck_w_top = r * 0.5f, neck_w_bot = r * 0.7f;
        float dx = neck.x - head.x, dy = neck.y - head.y;
        float len = sqrtf(dx * dx + dy * dy);
        if (len > 1.0f) {
            float nx = -dy / len, ny = dx / len;
            ImVec2 nt[4] = {
                {head.x + nx * neck_w_top, head.y + ny * neck_w_top},
                {head.x - nx * neck_w_top, head.y - ny * neck_w_top},
                {neck.x - nx * neck_w_bot, neck.y - ny * neck_w_bot},
                {neck.x + nx * neck_w_bot, neck.y + ny * neck_w_bot},
            };
            if (glow) {
                float expand = g_settings.glow_expand_inner;
                ImVec2 ntg[4] = {
                    {head.x + nx * (neck_w_top + expand), head.y + ny * (neck_w_top + expand)},
                    {head.x - nx * (neck_w_top + expand), head.y - ny * (neck_w_top + expand)},
                    {neck.x - nx * (neck_w_bot + expand), neck.y - ny * (neck_w_bot + expand)},
                    {neck.x + nx * (neck_w_bot + expand), neck.y + ny * (neck_w_bot + expand)},
                };
                d->AddQuadFilled(ntg[0], ntg[1], ntg[2], ntg[3], c.glow);
            }
            d->AddQuadFilled(nt[0], nt[1], nt[2], nt[3], c.head_fill);
            d->AddLine(nt[0], nt[3], c.outline, 1.0f);
            d->AddLine(nt[1], nt[2], c.outline, 1.0f);
        }
        if (glow) {
            d->AddCircleFilled(head, r + g_settings.glow_expand_outer, c.glow, 24);
            d->AddCircleFilled(head, r + g_settings.glow_expand_inner, c.glow, 24);
        }
        draw_ellipse_filled(d, head, r, r * 1.15f, 0, c.head_fill, 24);
        draw_ellipse_stroke(d, head, r, r * 1.15f, 0, c.outline, 1.2f, 24);
    }

    static void draw_head_wire(ImDrawList* d, const PlayerVisuals& p,
                               const ColorSet& c, float depth_scale) {
        if (!p.visible[BONE_HEAD] || !p.visible[BONE_NECK]) return;
        float r = std::clamp(g_settings.head_radius * depth_scale / p.depths[BONE_HEAD],
                             2.0f, 80.0f);
        ImVec2 head = p.screens[BONE_HEAD];
        ImVec2 neck = p.screens[BONE_NECK];
        draw_ellipse_filled(d, head, r, r * 1.15f, 0,
                            (c.fill & 0x00FFFFFF) | 0x10000000, 20);
        draw_ellipse_stroke(d, head, r, r * 1.15f, 0, c.wire, 1.3f, 20);
        d->AddLine({head.x - r, head.y}, {head.x + r, head.y}, c.wire, 0.5f);
        d->AddLine({head.x, head.y - r * 1.15f}, {head.x, head.y + r * 1.15f}, c.wire, 0.5f);
        d->AddLine(head, neck, c.wire, 1.0f);
    }

    // Head dot for skeleton style — just a small circle at the head bone
    static void draw_head_skeleton(ImDrawList* d, const PlayerVisuals& p,
                                   const ColorSet& c, float depth_scale) {
        if (!p.visible[BONE_HEAD]) return;
        float r = std::clamp(g_settings.head_radius * depth_scale / p.depths[BONE_HEAD],
                             2.0f, 80.0f);
        ImVec2 head = p.screens[BONE_HEAD];
        d->AddCircle(head, r, c.wire, 20, 1.2f);
    }

    // ===== SKELETON =====

    static void draw_skeleton(ImDrawList* d, const PlayerVisuals& p, const ColorSet& c) {
        ImU32 w = (c.wire & 0x00FFFFFF) | 0x60000000;
        for (const auto& [f, t] : SKELETON_CONNECTIONS)
            if (p.visible[f] && p.visible[t])
                d->AddLine(p.screens[f], p.screens[t], w, 1.0f);
    }

    // Skeleton style: slightly thicker/brighter lines than the old skeleton wire overlay
    static void draw_skeleton_style(ImDrawList* d, const PlayerVisuals& p, const ColorSet& c) {
        for (const auto& [f, t] : SKELETON_CONNECTIONS)
            if (p.visible[f] && p.visible[t])
                d->AddLine(p.screens[f], p.screens[t], c.wire, 1.5f);
    }

    // ===== MAIN DISPATCH =====

    static void draw_chams(ImDrawList* d, const PlayerVisuals& p,
                           const ColorSet& c, ChamsStyle style, float depth_scale) {
        switch (style) {
        case ChamsStyle::FILLED:
            draw_body_filled(d, p, c, false, depth_scale);
            draw_body_outline(d, p, c, depth_scale);
            if (g_settings.draw_head) draw_head(d, p, c, false, depth_scale);
            break;
        case ChamsStyle::WIREFRAME:
            draw_body_wireframe(d, p, c, depth_scale);
            if (g_settings.draw_head) draw_head_wire(d, p, c, depth_scale);
            break;
        case ChamsStyle::GLOW:
            draw_body_glow(d, p, c, depth_scale, g_settings.glow_expand_outer);
            draw_body_glow(d, p, c, depth_scale, g_settings.glow_expand_inner);
            draw_body_filled(d, p, c, false, depth_scale);
            draw_body_outline(d, p, c, depth_scale);
            if (g_settings.draw_head) draw_head(d, p, c, true, depth_scale);
            break;
        case ChamsStyle::SKELETON:
            draw_skeleton_style(d, p, c);
            if (g_settings.draw_head) draw_head_skeleton(d, p, c, depth_scale);
            break;
        }
    }

    static ImU32 slice_color(ImU32 col, int slices) {
        if (slices <= 1) return col;
        float a = ((col >> 24) & 0xFF) / 255.0f;
        float per = 1.0f - powf(1.0f - a, 1.0f / (float)slices);
        ImU32 pa = (ImU32)(per * 255.0f);
        return (col & 0x00FFFFFF) | (pa << 24);
    }
};