#define IMGUI_DEFINE_MATH_OPERATORS
#include "esp.h"
#include "scene.h"
#include "settings.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace lit::features::esp {

namespace {

bool project(const Camera& c, float x, float y, float z, ImVec2& out) {
    const auto& m = c.vp;
    const float px = m[0]*x + m[1]*y + m[2] *z + m[3];
    const float py = m[4]*x + m[5]*y + m[6] *z + m[7];
    const float pw = m[12]*x + m[13]*y + m[14]*z + m[15];
    if (pw <= 1e-6f) return false;

    const float inv_w = 1.f / pw;
    out.x = (c.width  * 0.5f) * (px * inv_w + 1.f);
    out.y = (c.height * 0.5f) * (1.f - py * inv_w);
    return true;
}

ImU32 health_color(float pct) {
    pct = pct < 0.f ? 0.f : (pct > 1.f ? 1.f : pct);
    const float r = pct < 0.5f ? 1.f : (1.f - pct) * 2.f;
    const float g = pct < 0.5f ? pct * 2.f : 1.f;
    return IM_COL32(int(r*255), int(g*255), 0, 255);
}

}

uint32_t with_alpha(uint32_t col, float alpha_mult) {
    const uint32_t a = (uint32_t)(((col >> 24) & 0xFF) * std::clamp(alpha_mult, 0.f, 1.f));
    return (col & 0x00FFFFFFu) | (a << 24);
}

uint32_t rainbow_color(double t) {
    float r, g, b;
    ImGui::ColorConvertHSVtoRGB((float)std::fmod(t * 0.15, 1.0), 0.85f, 1.f, r, g, b);
    return IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), 235);
}

void draw_corner_box(ImDrawList* dl, float tlx, float tly, float brx, float bry,
                      uint32_t col, float thickness) {
    const float len = std::min(brx - tlx, bry - tly) * 0.3f;
    dl->AddLine(ImVec2(tlx, tly), ImVec2(tlx + len, tly), col, thickness);
    dl->AddLine(ImVec2(tlx, tly), ImVec2(tlx, tly + len), col, thickness);
    dl->AddLine(ImVec2(brx, tly), ImVec2(brx - len, tly), col, thickness);
    dl->AddLine(ImVec2(brx, tly), ImVec2(brx, tly + len), col, thickness);
    dl->AddLine(ImVec2(tlx, bry), ImVec2(tlx + len, bry), col, thickness);
    dl->AddLine(ImVec2(tlx, bry), ImVec2(tlx, bry - len), col, thickness);
    dl->AddLine(ImVec2(brx, bry), ImVec2(brx - len, bry), col, thickness);
    dl->AddLine(ImVec2(brx, bry), ImVec2(brx, bry - len), col, thickness);
}

void draw_glow(ImDrawList* dl, float tlx, float tly, float brx, float bry,
               uint32_t col, float alpha_mult) {
    constexpr int kLayers = 4;
    for (int i = kLayers; i >= 1; --i) {
        const float expand = (float)i * 2.5f;
        const float a = alpha_mult * (0.10f * (float)(kLayers - i + 1) / (float)kLayers);
        dl->AddRect(ImVec2(tlx - expand, tly - expand),
                    ImVec2(brx + expand, bry + expand),
                    with_alpha(col, a), 4.f, 0, 3.f);
    }
}

void draw(ImDrawList* dl) {
    if (!dl) return;
    const auto& s = settings().esp;
    if (!s.enabled.load()) return;

    std::vector<Entity> ents;
    Camera cam{};
    scene().read(ents, cam);

    if (ents.empty() || cam.width <= 0.f || cam.height <= 0.f) return;

    const float max_d      = s.max_distance.load();
    const bool  draw_boxes = s.boxes.load();
    const bool  corner_box = s.corner_box.load();
    const bool  draw_fill  = s.fill.load();
    const bool  glow_on    = s.glow.load();
    const bool  rainbow    = s.rainbow.load();
    const bool  draw_name  = s.names.load();
    const bool  draw_hp    = s.health.load();
    const bool  draw_dist  = s.distance.load();
    const bool  draw_trace = s.tracers.load();
    const bool  team_check = s.team_check.load();
    const bool  fade       = s.fade_by_distance.load();

    const ImU32 base_col = rainbow ? rainbow_color(ImGui::GetTime())
                                   : (ImU32)s.color.load();
    const ImVec2 tracer_from(cam.width * 0.5f, cam.height);

    struct Item { const Entity* e; float dist; };
    std::vector<Item> items;
    items.reserve(ents.size());
    for (const auto& e : ents) {
        if (team_check && cam.local_team && e.team == cam.local_team) continue;
        const float dx = e.x - cam.cam_x;
        const float dy = e.y - cam.cam_y;
        const float dz = e.z - cam.cam_z;
        const float d = std::sqrt(dx*dx + dy*dy + dz*dz);
        if (d <= max_d) items.push_back({&e, d});
    }
    std::sort(items.begin(), items.end(),
        [](const Item& a, const Item& b) { return a.dist > b.dist; });

    for (const auto& it : items) {
        const auto& e = *it.e;

        ImVec2 head, feet;
        if (!project(cam, e.x, e.y + 3.f, e.z, head)) continue;
        if (!project(cam, e.x, e.y - 4.f, e.z, feet)) continue;

        const float h = feet.y - head.y;
        if (h < 6.f) continue;
        const float w = h * 0.45f;
        const float cx = (head.x + feet.x) * 0.5f;

        const ImVec2 tl(cx - w * 0.5f, head.y);
        const ImVec2 br(cx + w * 0.5f, feet.y);

        const float alpha_mult = fade
            ? std::clamp(1.f - (it.dist / max_d) * 0.6f, 0.4f, 1.f)
            : 1.f;

        if (glow_on) draw_glow(dl, tl.x, tl.y, br.x, br.y, base_col, alpha_mult);

        if (draw_fill) {
            dl->AddRectFilled(tl, br, with_alpha(base_col, alpha_mult * 0.18f), 2.f);
        }

        if (draw_boxes) {
            if (corner_box) {
                draw_corner_box(dl, tl.x, tl.y, br.x, br.y, with_alpha(IM_COL32(0, 0, 0, 220), alpha_mult), 4.f);
                draw_corner_box(dl, tl.x, tl.y, br.x, br.y, with_alpha(base_col, alpha_mult), 2.f);
            } else {
                dl->AddRect(tl, br, with_alpha(IM_COL32(0, 0, 0, 220), alpha_mult), 0.f, 0, 3.f);
                dl->AddRect(tl, br, with_alpha(base_col, alpha_mult), 0.f, 0, 1.f);
            }
        }

        if (draw_hp && e.max_health > 0.f) {
            const float pct = e.health / e.max_health;
            const ImVec2 bar_tl(tl.x - 6.f, tl.y);
            const ImVec2 bar_br(tl.x - 2.f, br.y);
            dl->AddRectFilled(bar_tl, bar_br, IM_COL32(0, 0, 0, 200));
            const float fill_h = (bar_br.y - bar_tl.y) * pct;
            dl->AddRectFilled(
                ImVec2(bar_tl.x, bar_br.y - fill_h),
                bar_br,
                health_color(pct));
        }

        if (draw_name && !e.name.empty()) {
            const ImVec2 ts = ImGui::CalcTextSize(e.name.c_str());
            const ImVec2 tp(cx - ts.x * 0.5f, tl.y - ts.y - 2.f);
            dl->AddText(ImVec2(tp.x + 1, tp.y + 1), IM_COL32(0, 0, 0, 220), e.name.c_str());
            dl->AddText(tp, IM_COL32(255, 255, 255, 235), e.name.c_str());
        }

        if (draw_dist) {
            char buf[24];
            std::snprintf(buf, sizeof(buf), "%.0fm", it.dist);
            const ImVec2 ts = ImGui::CalcTextSize(buf);
            const ImVec2 tp(cx - ts.x * 0.5f, br.y + 2.f);
            dl->AddText(ImVec2(tp.x + 1, tp.y + 1), IM_COL32(0, 0, 0, 220), buf);
            dl->AddText(tp, IM_COL32(200, 200, 210, 235), buf);
        }

        if (draw_trace) {
            dl->AddLine(tracer_from, ImVec2(cx, br.y), with_alpha(base_col, alpha_mult * 0.75f), 1.f);
        }
    }
}

}
