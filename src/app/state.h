#pragma once

#include <atomic>
#include <string>

namespace lit::app {

enum class Stage {
    Splash,
    Injecting,
    Ready,
};

struct State {
    std::atomic<Stage> stage{Stage::Splash};
    std::atomic<int>   progress{0};
    std::string        message;
};

State& state();

void inject();

void reset();

}
