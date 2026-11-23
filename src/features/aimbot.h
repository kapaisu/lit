#pragma once

#include <cstdint>

struct ImDrawList;

namespace lit::features::aimbot {

enum class Stage : int {
    Disabled,
    KeyWaiting,
    NoTargets,
    Locked,
};

struct Diag {
    Stage    stage       = Stage::Disabled;
    uint32_t candidates  = 0;
    uint32_t in_fov      = 0;
    uint32_t writes_ok   = 0;
    uint32_t writes_fail = 0;
    uintptr_t locked     = 0;
    float    cursor_x    = 0.f;
    float    cursor_y    = 0.f;
    float    target_sx   = 0.f;
    float    target_sy   = 0.f;
};

const Diag& diag();

void tick();
void draw(ImDrawList* dl);

}
