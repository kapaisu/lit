#define NOMINMAX
#include "movement.h"
#include "settings.h"
#include "../roblox/roblox.h"
#include "../roblox/offsets.h"
#include "../roblox/instance.h"
#include "../core/memory.h"

#include <windows.h>
#include <chrono>
#include <cmath>

namespace lit::features::movement {

namespace {

bool key_down(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

uintptr_t local_character() {
    const auto& c = roblox::ctx();
    if (!c.local_player) return 0;
    const uint32_t o_model = roblox::offsets::get("Player", "ModelInstance");
    if (!o_model) return 0;
    return memory::read<uintptr_t>(c.proc, c.local_player + o_model);
}

uintptr_t local_root(uintptr_t character) {
    if (!character) return 0;
    uintptr_t hrp = roblox::instance::find_child_by_name(character, "HumanoidRootPart");
    if (!hrp) hrp = roblox::instance::find_child_by_name(character, "Torso");
    if (!hrp) hrp = roblox::instance::find_child_by_name(character, "UpperTorso");
    return hrp;
}

uintptr_t local_humanoid(uintptr_t character) {
    if (!character) return 0;
    uintptr_t h = roblox::instance::find_child_by_class(character, "Humanoid");
    if (!h) h = roblox::instance::find_child_by_name(character, "Humanoid");
    return h;
}

bool camera_axes(float right[3], float up[3], float back[3]) {
    const auto& c = roblox::ctx();
    if (!c.camera) return false;
    const uint32_t o_rot = roblox::offsets::get("Camera", "Rotation");
    if (!o_rot) return false;

    float m[9]{};
    if (!memory::read_raw(c.proc, c.camera + o_rot, m, sizeof(m))) return false;
    right[0]=m[0]; right[1]=m[3]; right[2]=m[6];
    up   [0]=m[1]; up   [1]=m[4]; up   [2]=m[7];
    back [0]=m[2]; back [1]=m[5]; back [2]=m[8];
    return true;
}

void normalize2(float v[3]) {
    const float l = std::sqrt(v[0]*v[0] + v[2]*v[2]);
    if (l > 1e-6f) { v[0] /= l; v[2] /= l; v[1] = 0.f; }
}

bool g_fly_last = false;

void tick_fly(float dt) {
    const bool on = settings().movement.fly.load();

    if (on != g_fly_last) {
        const uintptr_t character = local_character();
        const uintptr_t humanoid  = local_humanoid(character);
        if (humanoid) {
            const uint32_t o_ps = roblox::offsets::get("Humanoid", "PlatformStand");
            if (o_ps) {
                const uint8_t v = on ? 1 : 0;
                memory::write_raw(roblox::ctx().proc, humanoid + o_ps, &v, 1);
            }
        }
        g_fly_last = on;
    }
    if (!on) return;

    const uintptr_t character = local_character();
    const uintptr_t root      = local_root(character);
    if (!root) return;

    if (const uintptr_t humanoid = local_humanoid(character)) {
        const uint32_t o_ps = roblox::offsets::get("Humanoid", "PlatformStand");
        if (o_ps) {
            const uint8_t v = 1;
            memory::write_raw(roblox::ctx().proc, humanoid + o_ps, &v, 1);
        }
    }

    const uint32_t o_prim = roblox::offsets::get("BasePart", "Primitive");
    const uint32_t o_pos  = roblox::offsets::get("Primitive", "Position");
    const uint32_t o_vel  = roblox::offsets::get("Primitive", "AssemblyLinearVelocity");
    if (!o_prim || !o_pos) return;

    const uintptr_t prim = memory::read<uintptr_t>(roblox::ctx().proc, root + o_prim);
    if (!prim) return;

    float pos[3]{};
    if (!memory::read_raw(roblox::ctx().proc, prim + o_pos, pos, sizeof(pos))) return;

    float right[3], up[3], back[3];
    if (!camera_axes(right, up, back)) return;

    float fwd[3] = { -back[0], 0.f, -back[2] };
    normalize2(fwd);
    float side[3] = { right[0], 0.f, right[2] };
    normalize2(side);

    float dx = 0.f, dy = 0.f, dz = 0.f;
    if (key_down('W')) { dx += fwd[0]; dz += fwd[2]; }
    if (key_down('S')) { dx -= fwd[0]; dz -= fwd[2]; }
    if (key_down('D')) { dx += side[0]; dz += side[2]; }
    if (key_down('A')) { dx -= side[0]; dz -= side[2]; }
    if (key_down(VK_SPACE))    dy += 1.f;
    if (key_down(VK_LSHIFT))   dy -= 1.f;

    const float speed = settings().movement.fly_speed.load();
    pos[0] += dx * speed * dt;
    pos[1] += dy * speed * dt;
    pos[2] += dz * speed * dt;

    memory::write_raw(roblox::ctx().proc, prim + o_pos, pos, sizeof(pos));

    if (o_vel) {
        const float zero[3]{};
        memory::write_raw(roblox::ctx().proc, prim + o_vel, zero, sizeof(zero));
    }
}

bool g_noclip_last = false;

void set_collide(uintptr_t prim, uint32_t o_flags, bool collide) {
    uint8_t flags = memory::read<uint8_t>(roblox::ctx().proc, prim + o_flags);
    if (collide) flags |= 0x8; else flags &= ~0x8;
    memory::write_raw(roblox::ctx().proc, prim + o_flags, &flags, 1);
}

void tick_noclip() {
    const bool on = settings().movement.noclip.load();
    if (!on && !g_noclip_last) return;

    const uintptr_t character = local_character();
    if (!character) return;

    const uint32_t o_prim  = roblox::offsets::get("BasePart", "Primitive");
    const uint32_t o_flags = roblox::offsets::get("Primitive", "Flags");
    if (!o_prim || !o_flags) return;

    for (uintptr_t part : roblox::instance::children(character, 64)) {
        const uintptr_t prim = memory::read<uintptr_t>(roblox::ctx().proc, part + o_prim);
        if (!prim) continue;
        set_collide(prim, o_flags, !on);
    }
    g_noclip_last = on;
}

void tick_infjump() {
    if (!settings().movement.infinite_jump.load()) return;
    if (!key_down(VK_SPACE)) return;

    const uintptr_t character = local_character();
    const uintptr_t humanoid  = local_humanoid(character);
    if (!humanoid) return;

    const uint32_t o_jump = roblox::offsets::get("Humanoid", "Jump");
    if (!o_jump) return;
    const uint8_t v = 1;
    memory::write_raw(roblox::ctx().proc, humanoid + o_jump, &v, 1);
}

bool  g_jp_active_prev = false;
float g_jp_original    = 50.f;

constexpr float kMaxSafeSpeed = 150.f;

void tick_walk_via_velocity() {
    const float target = settings().movement.walk_speed.load();
    if (std::abs(target - 16.f) < 0.5f) return;
    const float clamped = std::min(target, kMaxSafeSpeed);

    const uintptr_t character = local_character();
    const uintptr_t root      = local_root(character);
    if (!root) return;

    float right[3], up[3], back[3];
    if (!camera_axes(right, up, back)) return;
    float fwd[3]  = { -back[0], 0.f, -back[2] };  normalize2(fwd);
    float side[3] = {  right[0], 0.f, right[2] }; normalize2(side);

    float dx = 0.f, dz = 0.f;
    if (key_down('W')) { dx += fwd[0];  dz += fwd[2]; }
    if (key_down('S')) { dx -= fwd[0];  dz -= fwd[2]; }
    if (key_down('D')) { dx += side[0]; dz += side[2]; }
    if (key_down('A')) { dx -= side[0]; dz -= side[2]; }

    const float mag = std::sqrt(dx*dx + dz*dz);
    if (mag < 1e-3f) return;

    const uint32_t o_prim = roblox::offsets::get("BasePart", "Primitive");
    const uint32_t o_vel  = roblox::offsets::get("Primitive", "AssemblyLinearVelocity");
    if (!o_prim || !o_vel) return;

    const uintptr_t prim = memory::read<uintptr_t>(roblox::ctx().proc, root + o_prim);
    if (!prim) return;

    float vel[3]{};
    if (!memory::read_raw(roblox::ctx().proc, prim + o_vel, vel, sizeof(vel))) return;
    vel[0] = (dx / mag) * clamped;
    vel[2] = (dz / mag) * clamped;

    memory::write_raw(roblox::ctx().proc, prim + o_vel, vel, sizeof(vel));
}

void tick_jump_power() {
    const float jp = settings().movement.jump_power.load();
    const bool  jp_active = std::abs(jp - 50.f) > 0.5f;

    const uintptr_t character = local_character();
    const uintptr_t humanoid  = local_humanoid(character);
    if (!humanoid) return;

    const uint32_t o_jp = roblox::offsets::get("Humanoid", "JumpPower");
    if (!o_jp) return;

    if (jp_active) {
        if (!g_jp_active_prev) {
            g_jp_original = memory::read<float>(roblox::ctx().proc, humanoid + o_jp);
            if (g_jp_original < 1.f || g_jp_original > 500.f) g_jp_original = 50.f;
        }
        memory::write<float>(roblox::ctx().proc, humanoid + o_jp, jp);
    } else if (g_jp_active_prev) {
        memory::write<float>(roblox::ctx().proc, humanoid + o_jp, g_jp_original);
    }
    g_jp_active_prev = jp_active;
}

}

void tick() {
    if (!roblox::ctx().ready.load()) return;

    using clock = std::chrono::steady_clock;
    static clock::time_point last{};
    const auto now = clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - last).count();
    constexpr auto period_us = 16666;
    if (elapsed < period_us) { tick_infjump(); return; }
    const float dt = std::min(0.1f, (float)elapsed / 1e6f);
    last = now;

    tick_fly(dt);
    tick_noclip();
    tick_infjump();
    tick_walk_via_velocity();
    tick_jump_power();
}

}
