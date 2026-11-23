#include "scene.h"
#include "settings.h"
#include "../roblox/roblox.h"
#include "../roblox/offsets.h"
#include "../roblox/instance.h"
#include "../core/memory.h"

namespace lit::features {

SceneSnapshot& scene() {
    static SceneSnapshot s;
    return s;
}

SceneDiag& scene_diag() {
    static SceneDiag d;
    return d;
}

void SceneSnapshot::write(std::vector<Entity>&& e, const Camera& c) {
    std::lock_guard lk(m_);
    entities_ = std::move(e);
    camera_ = c;
}

void SceneSnapshot::read(std::vector<Entity>& out_entities, Camera& out_camera) const {
    std::lock_guard lk(m_);
    out_entities = entities_;
    out_camera = camera_;
}

namespace {

bool read_part(uintptr_t character, std::string_view part_name,
               float& x, float& y, float& z,
               float& vx, float& vy, float& vz)
{
    if (!character) return false;
    uintptr_t part = roblox::instance::find_child_by_name(character, part_name);
    if (!part) return false;
    const uint32_t o_prim = roblox::offsets::get("BasePart", "Primitive");
    const uint32_t o_pos  = roblox::offsets::get("Primitive", "Position");
    const uint32_t o_vel  = roblox::offsets::get("Primitive", "AssemblyLinearVelocity");
    if (!o_prim || !o_pos) return false;
    const uintptr_t prim = memory::read<uintptr_t>(roblox::ctx().proc, part + o_prim);
    if (!prim) return false;
    float p[3]{};
    if (!memory::read_raw(roblox::ctx().proc, prim + o_pos, p, sizeof(p))) return false;
    x = p[0]; y = p[1]; z = p[2];
    if (o_vel) {
        float v[3]{};
        if (memory::read_raw(roblox::ctx().proc, prim + o_vel, v, sizeof(v))) {
            vx = v[0]; vy = v[1]; vz = v[2];
        }
    }
    return true;
}

bool read_root_position(uintptr_t character, float& x, float& y, float& z) {
    float vx, vy, vz;
    return read_part(character, "HumanoidRootPart", x, y, z, vx, vy, vz)
        || read_part(character, "Torso",           x, y, z, vx, vy, vz)
        || read_part(character, "UpperTorso",      x, y, z, vx, vy, vz);
}

bool read_aim_part(uintptr_t character, int hitbox,
                   float& x, float& y, float& z,
                   float& vx, float& vy, float& vz)
{
    vx = vy = vz = 0.f;
    switch (hitbox) {
        case 0:
            if (read_part(character, "Head", x, y, z, vx, vy, vz)) return true;
            break;
        case 1:
            if (read_part(character, "Head", x, y, z, vx, vy, vz)) { y -= 0.5f; return true; }
            break;
        case 2:
            if (read_part(character, "UpperTorso", x, y, z, vx, vy, vz)) return true;
            if (read_part(character, "Torso",      x, y, z, vx, vy, vz)) return true;
            break;
        case 3:
            if (read_part(character, "LowerTorso", x, y, z, vx, vy, vz)) return true;
            if (read_part(character, "Torso",      x, y, z, vx, vy, vz)) { y -= 1.0f; return true; }
            break;
        default: break;
    }

    return read_part(character, "HumanoidRootPart", x, y, z, vx, vy, vz)
        || read_part(character, "Torso",           x, y, z, vx, vy, vz)
        || read_part(character, "UpperTorso",      x, y, z, vx, vy, vz);
}

bool read_humanoid(uintptr_t character, float& health, float& max_health) {
    if (!character) return false;

    uintptr_t hum = roblox::instance::find_child_by_class(character, "Humanoid");
    if (!hum) hum = roblox::instance::find_child_by_name(character, "Humanoid");
    if (!hum) return false;
    const uint32_t o_h  = roblox::offsets::get("Humanoid", "Health");
    const uint32_t o_mh = roblox::offsets::get("Humanoid", "MaxHealth");
    if (!o_h || !o_mh) return false;
    health     = memory::read<float>(roblox::ctx().proc, hum + o_h);
    max_health = memory::read<float>(roblox::ctx().proc, hum + o_mh);
    return true;
}

void read_camera(Camera& out) {
    const auto& c = roblox::ctx();
    if (!c.proc.h) return;

    const uint32_t o_visual = roblox::offsets::get("VisualEngine", "Pointer");
    const uint32_t o_vm     = roblox::offsets::get("VisualEngine", "ViewMatrix");
    const uint32_t o_dim    = roblox::offsets::get("VisualEngine", "Dimensions");
    if (!o_visual) return;
    const uintptr_t visual = memory::read<uintptr_t>(c.proc, c.proc.base + o_visual);
    if (!visual) return;

    if (o_vm) memory::read_raw(c.proc, visual + o_vm, out.vp.data(), sizeof(out.vp));
    if (o_dim) {
        float dim[2]{};
        if (memory::read_raw(c.proc, visual + o_dim, dim, sizeof(dim))) {
            out.width  = dim[0];
            out.height = dim[1];
        }
    }

    if (out.width <= 0.f || out.height <= 0.f) {
        const uint32_t o_vp = roblox::offsets::get("Camera", "ViewportSize");
        if (c.camera && o_vp) {
            float vp[2]{};
            if (memory::read_raw(c.proc, c.camera + o_vp, vp, sizeof(vp))) {
                out.width  = vp[0];
                out.height = vp[1];
            }
        }
    }

    const uint32_t o_pos = roblox::offsets::get("Camera", "Position");
    if (c.camera && o_pos) {
        float p[3]{};
        if (memory::read_raw(c.proc, c.camera + o_pos, p, sizeof(p))) {
            out.cam_x = p[0]; out.cam_y = p[1]; out.cam_z = p[2];
        }
    }
}

}

void refresh_scene() {

    const bool esp_on = settings().esp.enabled.load();
    const bool aim_on = settings().aimbot.enabled.load();
    if (!esp_on && !aim_on) {
        scene().write({}, Camera{});
        return;
    }
    if (!roblox::ctx().ready.load() || !roblox::ctx().players) return;

    auto& diag = scene_diag();
    diag.raw_children = 0;
    diag.skipped_local = 0;
    diag.no_model = 0;
    diag.no_root = 0;
    diag.added = 0;

    const uint32_t o_model = roblox::offsets::get("Player", "ModelInstance");
    const uint32_t o_team  = roblox::offsets::get("Player", "Team");
    if (!o_model) return;

    const uintptr_t local = roblox::ctx().local_player;
    const uintptr_t local_team = (local && o_team)
        ? memory::read<uintptr_t>(roblox::ctx().proc, local + o_team)
        : 0;

    const int aim_hitbox = settings().aimbot.hitbox.load();

    Camera cam{};
    read_camera(cam);
    cam.local_team = local_team;

    std::vector<Entity> ents;
    const auto players = roblox::instance::children(roblox::ctx().players, 256);
    diag.raw_children = (uint32_t)players.size();
    for (uintptr_t player : players) {
        if (player == local) { ++diag.skipped_local; continue; }

        const uintptr_t character = memory::read<uintptr_t>(
            roblox::ctx().proc, player + o_model);
        if (!character) { ++diag.no_model; continue; }

        Entity e{};
        e.player = player;
        e.team = (o_team ? memory::read<uintptr_t>(roblox::ctx().proc, player + o_team) : 0);

        if (!read_root_position(character, e.x, e.y, e.z)) { ++diag.no_root; continue; }
        if (!read_aim_part(character, aim_hitbox,
                           e.aim_x, e.aim_y, e.aim_z,
                           e.vx, e.vy, e.vz)) {
            e.aim_x = e.x; e.aim_y = e.y; e.aim_z = e.z;
        }
        read_humanoid(character, e.health, e.max_health);
        if (e.max_health <= 0.f) { e.health = 0.f; e.max_health = 100.f; }
        e.name = roblox::instance::name(player);
        ++diag.added;
        ents.emplace_back(std::move(e));
    }
    scene().write(std::move(ents), cam);
}

}
