#pragma once

#include <atomic>
#include <cstdint>

namespace lit::features {

struct Aimbot {
    std::atomic<bool>  enabled{false};
    std::atomic<bool>  silent{false};
    std::atomic<bool>  sticky{false};
    std::atomic<bool>  team_check{true};
    std::atomic<bool>  draw_fov{false};
    std::atomic<int>   aim_mode{0};
    std::atomic<int>   aim_key{0x02};
    std::atomic<int>   hitbox{0};
    std::atomic<float> fov{120.f};
    std::atomic<float> smoothing{1.f};
    std::atomic<float> sensitivity{2.f};
    std::atomic<float> prediction{0.f};
    std::atomic<float> max_distance{2000.f};
};

struct Esp {
    std::atomic<bool> enabled{false};
    std::atomic<bool> boxes{true};
    std::atomic<bool> corner_box{false};
    std::atomic<bool> fill{false};
    std::atomic<bool> glow{false};
    std::atomic<bool> rainbow{false};
    std::atomic<uint32_t> color{0xFFFFFFFFu};
    std::atomic<bool> health{true};
    std::atomic<bool> names{true};
    std::atomic<bool> distance{true};
    std::atomic<bool> tracers{false};
    std::atomic<bool> team_check{true};
    std::atomic<bool> fade_by_distance{true};
    std::atomic<float> max_distance{2500.f};
};

struct Movement {
    std::atomic<bool>  fly{false};
    std::atomic<bool>  noclip{false};
    std::atomic<bool>  infinite_jump{false};
    std::atomic<float> walk_speed{16.f};
    std::atomic<float> jump_power{50.f};
    std::atomic<float> fly_speed{60.f};
};

struct Misc {
    std::atomic<bool> anti_afk{false};
    std::atomic<bool> fullbright{false};
    std::atomic<bool> capture_proof{false};
    std::atomic<int>  panic_key{0x23};
};

struct All {
    Aimbot aimbot;
    Esp esp;
    Movement movement;
    Misc misc;
};

All& settings();

}
