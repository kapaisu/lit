#pragma once

#include "../core/process.h"
#include <atomic>
#include <cstdint>

namespace lit::roblox {

struct Context {
    process::Handle proc{};
    uintptr_t data_model    = 0;
    uintptr_t workspace     = 0;
    uintptr_t players       = 0;
    uintptr_t local_player  = 0;
    uintptr_t camera        = 0;
    uintptr_t mouse_service = 0;
    std::atomic<bool> ready{false};
};

Context& ctx();

bool attach();

void refresh_pointers();

void detach();

}
