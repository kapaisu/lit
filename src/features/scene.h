#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace lit::features {

struct Entity {
    float       x, y, z;
    float       aim_x, aim_y, aim_z;
    float       vx, vy, vz;
    float       health;
    float       max_health;
    uintptr_t   player;
    uintptr_t   team;
    std::string name;
};

struct Camera {
    std::array<float, 16> vp{};
    float                 width  = 0.f;
    float                 height = 0.f;
    float                 cam_x = 0.f, cam_y = 0.f, cam_z = 0.f;
    uintptr_t             local_team = 0;
};

class SceneSnapshot {
public:
    void write(std::vector<Entity>&& e, const Camera& c);
    void read(std::vector<Entity>& out_entities, Camera& out_camera) const;

private:
    mutable std::mutex   m_;
    std::vector<Entity>  entities_;
    Camera               camera_{};
};

SceneSnapshot& scene();

struct SceneDiag {
    std::atomic<uint32_t> raw_children{0};
    std::atomic<uint32_t> skipped_local{0};
    std::atomic<uint32_t> no_model{0};
    std::atomic<uint32_t> no_root{0};
    std::atomic<uint32_t> added{0};
};
SceneDiag& scene_diag();

void refresh_scene();

}
