#include "feature_manager.h"
#include "settings.h"
#include "scene.h"
#include "aimbot.h"
#include "movement.h"
#include "../roblox/roblox.h"
#include "../roblox/offsets.h"
#include "../app/state.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace lit::features {

All& settings() {
    static All s;
    return s;
}

namespace {
    std::thread          worker;
    std::atomic<bool>    running{false};

    void tick_once() {

        if (app::state().stage.load() != app::Stage::Ready) return;

        if (roblox::ctx().proc.h) {
            DWORD ec = 0;
            if (GetExitCodeProcess(roblox::ctx().proc.h, &ec) && ec != STILL_ACTIVE) {
                app::reset();
                return;
            }
        }

        roblox::refresh_pointers();
        if (!roblox::ctx().ready.load()) return;

        refresh_scene();

        aimbot::tick();

        movement::tick();
    }

    void loop() {
        using namespace std::chrono;
        constexpr auto period = milliseconds(4);
        while (running.load()) {
            const auto t0 = steady_clock::now();
            tick_once();
            const auto slept = duration_cast<milliseconds>(steady_clock::now() - t0);
            if (slept < period) std::this_thread::sleep_for(period - slept);
        }
    }
}

void start() {
    if (running.exchange(true)) return;

    worker = std::thread(loop);
}

void stop() {
    if (!running.exchange(false)) return;
    if (worker.joinable()) worker.join();
}

}
