#define IMGUI_DEFINE_MATH_OPERATORS
#define NOMINMAX
#include "aimbot.h"
#include "scene.h"
#include "settings.h"
#include "../roblox/roblox.h"
#include "../roblox/offsets.h"
#include "../core/memory.h"

#include <imgui.h>
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cmath>

namespace lit::features::aimbot {

namespace {

std::atomic<uintptr_t> g_locked{0};

Diag g_diag;
}

const Diag& diag() { return g_diag; }

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

bool get_roblox_cursor(ImVec2& out) {
    POINT p;
    if (!GetCursorPos(&p)) return false;
    HWND rbx = FindWindowW(nullptr, L"Roblox");
    if (rbx && ScreenToClient(rbx, &p)) {
        out.x = (float)p.x;
        out.y = (float)p.y;
        return true;
    }
    out.x = (float)p.x;
    out.y = (float)p.y;
    return true;
}

void normalize3(float& x, float& y, float& z) {
    const float l = std::sqrt(x*x + y*y + z*z);
    if (l > 1e-6f) { const float inv = 1.f/l; x*=inv; y*=inv; z*=inv; }
}
void cross(float ax, float ay, float az, float bx, float by, float bz,
           float& ox, float& oy, float& oz)
{
    ox = ay*bz - az*by;
    oy = az*bx - ax*bz;
    oz = ax*by - ay*bx;
}

bool write_camera_rotation(uintptr_t camera_inst, float t_pos[3], float blend) {
    const uint32_t o_pos = roblox::offsets::get("Camera", "Position");
    const uint32_t o_rot = roblox::offsets::get("Camera", "Rotation");
    if (!o_pos || !o_rot) return false;

    float cp[3];
    if (!memory::read_raw(roblox::ctx().proc, camera_inst + o_pos, cp, 12))
        return false;

    float fx = t_pos[0] - cp[0], fy = t_pos[1] - cp[1], fz = t_pos[2] - cp[2];
    normalize3(fx, fy, fz);
    float bx = -fx, by = -fy, bz = -fz;

    float rx, ry, rz;
    cross(0.f, 1.f, 0.f, bx, by, bz, rx, ry, rz);
    normalize3(rx, ry, rz);

    float ux, uy, uz;
    cross(bx, by, bz, rx, ry, rz, ux, uy, uz);

    float target[9] = { rx, ux, bx,
                        ry, uy, by,
                        rz, uz, bz };

    float cur[9]{};
    if (blend < 1.f) {
        if (!memory::read_raw(roblox::ctx().proc, camera_inst + o_rot, cur, 36))
            return false;
        for (int i = 0; i < 9; ++i)
            target[i] = cur[i] * (1.f - blend) + target[i] * blend;

        float nr[3] = { target[0], target[3], target[6] }; normalize3(nr[0], nr[1], nr[2]);
        float nu[3] = { target[1], target[4], target[7] };
        float nb[3] = { target[2], target[5], target[8] }; normalize3(nb[0], nb[1], nb[2]);

        cross(nu[0], nu[1], nu[2], nb[0], nb[1], nb[2], nr[0], nr[1], nr[2]);
        normalize3(nr[0], nr[1], nr[2]);
        cross(nb[0], nb[1], nb[2], nr[0], nr[1], nr[2], nu[0], nu[1], nu[2]);
        target[0]=nr[0]; target[3]=nr[1]; target[6]=nr[2];
        target[1]=nu[0]; target[4]=nu[1]; target[7]=nu[2];
        target[2]=nb[0]; target[5]=nb[1]; target[8]=nb[2];
    }

    return memory::write_raw(roblox::ctx().proc, camera_inst + o_rot, target, 36);
}

bool silent_aim_write(float sx, float sy) {
    const auto& c = roblox::ctx();
    if (!c.mouse_service) return false;

    const uint32_t o_input = roblox::offsets::get("MouseService", "InputObject");
    const uint32_t o_pos   = roblox::offsets::get("MouseService", "MousePosition");
    if (!o_input || !o_pos) return false;

    const uintptr_t input = memory::read<uintptr_t>(c.proc, c.mouse_service + o_input);
    if (!input) return false;

    const float pos[2] = { sx, sy };
    return memory::write_raw(c.proc, input + o_pos, pos, sizeof(pos));
}

void move_cursor(float dx, float dy, float sensitivity, float smoothing) {

    const float scale = std::max(0.01f, sensitivity) /
                        std::max(0.01f, smoothing);
    float mx = dx * scale;
    float my = dy * scale;
    if      (mx >  100.f) mx =  100.f;
    else if (mx < -100.f) mx = -100.f;
    if      (my >  100.f) my =  100.f;
    else if (my < -100.f) my = -100.f;
    if ((int)mx == 0 && (int)my == 0) return;
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dx = (LONG)mx;
    in.mi.dy = (LONG)my;
    in.mi.dwFlags = MOUSEEVENTF_MOVE;
    SendInput(1, &in, sizeof(in));
}

}

void tick() {
    const auto& s = settings().aimbot;
    if (!s.enabled.load()) { g_locked.store(0); g_diag.stage = Stage::Disabled; return; }

    const int key = s.aim_key.load();
    if (key != 0 && !(GetAsyncKeyState(key) & 0x8000)) {
        g_locked.store(0);
        g_diag.stage = Stage::KeyWaiting;
        return;
    }

    std::vector<Entity> ents;
    Camera cam{};
    scene().read(ents, cam);
    if (ents.empty() || cam.width <= 0.f) {
        g_diag.stage = Stage::NoTargets;
        g_diag.candidates = 0;
        g_diag.in_fov     = 0;
        return;
    }

    const bool  team_check = s.team_check.load();
    const bool  sticky     = s.sticky.load();
    const bool  silent     = s.silent.load();
    const int   mode       = s.aim_mode.load();
    const float fov_px     = s.fov.load();
    const float max_d      = s.max_distance.load();
    const float smoothing  = std::max(1.f, s.smoothing.load());
    const float pred_t     = s.prediction.load();
    const float user_sens  = s.sensitivity.load();

    ImVec2 aim_point;
    if (!get_roblox_cursor(aim_point)) {
        aim_point.x = cam.width  * 0.5f;
        aim_point.y = cam.height * 0.5f;
    }
    g_diag.cursor_x = aim_point.x;
    g_diag.cursor_y = aim_point.y;

    const Entity* best = nullptr;
    float best_score = fov_px;
    const uintptr_t locked = sticky ? g_locked.load() : 0;
    uint32_t candidates = 0, in_fov = 0;

    for (const auto& e : ents) {
        if (team_check && cam.local_team && e.team == cam.local_team) continue;

        if (e.max_health > 0.f && e.health <= 0.f && e.max_health != 100.f) continue;

        const float dx = e.x - cam.cam_x;
        const float dy = e.y - cam.cam_y;
        const float dz = e.z - cam.cam_z;
        if (std::sqrt(dx*dx + dy*dy + dz*dz) > max_d) continue;
        ++candidates;

        const float px = e.aim_x + e.vx * pred_t;
        const float py = e.aim_y + e.vy * pred_t;
        const float pz = e.aim_z + e.vz * pred_t;
        ImVec2 sp;
        if (!project(cam, px, py, pz, sp)) continue;

        const float sdx = sp.x - aim_point.x;
        const float sdy = sp.y - aim_point.y;
        const float pd = std::sqrt(sdx*sdx + sdy*sdy);
        if (pd <= fov_px) ++in_fov;

        if (sticky && locked && e.player == locked && pd <= fov_px) {
            best = &e; break;
        }
        if (pd < best_score) { best_score = pd; best = &e; }
    }
    g_diag.candidates = candidates;
    g_diag.in_fov     = in_fov;

    if (!best) {
        g_locked.store(0);
        g_diag.stage = Stage::NoTargets;
        return;
    }
    g_locked.store(best->player);
    g_diag.locked = best->player;

    const float px = best->aim_x + best->vx * pred_t;
    const float py = best->aim_y + best->vy * pred_t;
    const float pz = best->aim_z + best->vz * pred_t;
    { ImVec2 tsp; if (project(cam, px, py, pz, tsp)) { g_diag.target_sx = tsp.x; g_diag.target_sy = tsp.y; } }

    if (silent) {
        ImVec2 sp;
        if (project(cam, px, py, pz, sp)) silent_aim_write(sp.x, sp.y);
    }

    bool ok = false;
    if (mode == 0) {

        ImVec2 sp;
        if (!project(cam, px, py, pz, sp)) return;

        const float sdx = sp.x - aim_point.x;
        const float sdy = sp.y - aim_point.y;
        const float pixel_delta = std::sqrt(sdx*sdx + sdy*sdy);
        if (pixel_delta < 1.f) {

            g_diag.stage = Stage::Locked;
            return;
        }

        float target[3] = { px, py, pz };
        const float blend = 1.f / smoothing;
        ok = write_camera_rotation(roblox::ctx().camera, target, blend > 1.f ? 1.f : blend);
    } else {
        ImVec2 sp;
        if (project(cam, px, py, pz, sp)) {
            move_cursor(sp.x - aim_point.x, sp.y - aim_point.y, user_sens, smoothing);
            ok = true;
        }
    }
    if (ok) ++g_diag.writes_ok; else ++g_diag.writes_fail;
    g_diag.stage = Stage::Locked;
}

void draw(ImDrawList* dl) {
    if (!dl) return;
    const auto& s = settings().aimbot;
    if (!s.enabled.load() || !s.draw_fov.load()) return;

    Camera cam{};
    std::vector<Entity> tmp;
    scene().read(tmp, cam);
    if (cam.width <= 0.f) return;

    ImVec2 aim_point;
    if (!get_roblox_cursor(aim_point)) {
        aim_point.x = cam.width  * 0.5f;
        aim_point.y = cam.height * 0.5f;
    }

    const float radius = s.fov.load();
    if (radius <= 0.f) return;
    dl->AddCircle(aim_point, radius, IM_COL32(0, 0, 0, 180), 96, 2.f);
    dl->AddCircle(aim_point, radius, IM_COL32(255, 255, 255, 220), 96, 1.f);
}

}
