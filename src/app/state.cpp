#include "state.h"
#include "../roblox/roblox.h"
#include "../roblox/offsets.h"

#include <chrono>
#include <thread>

namespace lit::app {

State& state() {
    static State s;
    return s;
}

void inject() {
    auto& st = state();

    Stage expected = Stage::Splash;
    if (!st.stage.compare_exchange_strong(expected, Stage::Injecting)) return;

    std::thread([&st]{
        st.progress.store(5);
        st.message.clear();

        using clock = std::chrono::steady_clock;
        const auto deadline = clock::now() + std::chrono::seconds(10);
        bool attached = false;
        while (clock::now() < deadline) {
            if (roblox::attach()) { attached = true; break; }
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            int p = 5 + int(65.0 * (double)std::chrono::duration_cast<std::chrono::milliseconds>(
                (clock::now() - (deadline - std::chrono::seconds(10)))).count() / 10000.0);
            if (p > 70) p = 70;
            st.progress.store(p);
        }
        if (!attached) {
            st.message = "roblox not found — launch it and try again";
            st.stage.store(Stage::Splash);
            return;
        }
        st.progress.store(75);

        const auto& os = roblox::offsets::status();
        if (os.count == 0) {
            st.message = os.error.empty() ? "offset fetch failed" : os.error;
            roblox::detach();
            st.stage.store(Stage::Splash);
            return;
        }
        st.progress.store(85);

        roblox::refresh_pointers();
        if (!roblox::ctx().ready.load())
            st.message = "attached, waiting for data model";

        st.progress.store(100);
        st.stage.store(Stage::Ready);
    }).detach();
}

void reset() {
    roblox::detach();
    auto& st = state();
    st.progress.store(0);
    st.message.clear();
    st.stage.store(Stage::Splash);
}

}
