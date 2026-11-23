#include "overlay.h"
#include "../features/settings.h"

#include <atomic>
#include <chrono>
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")

#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

namespace lit::app::overlay {

namespace {
    HWND g_self = nullptr;

    HWND g_roblox = nullptr;
    std::chrono::steady_clock::time_point g_roblox_last_lookup{};

    std::atomic<bool> g_visible{true};
    std::atomic<int>  g_menu_key{VK_INSERT};
    bool g_last_visible = true;
    bool g_last_capture_proof = false;

    void apply_exstyle(bool click_through) {
        LONG_PTR ex = WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
        if (click_through) ex |= WS_EX_TRANSPARENT;
        SetWindowLongPtrW(g_self, GWL_EXSTYLE, ex);
        SetLayeredWindowAttributes(g_self, 0, 255, LWA_ALPHA);
        MARGINS m{ -1, -1, -1, -1 };
        DwmExtendFrameIntoClientArea(g_self, &m);
    }

    bool is_fullscreen(HWND hw) {
        if (!hw) return false;
        MONITORINFO mi{}; mi.cbSize = sizeof(mi);
        HMONITOR mon = MonitorFromWindow(hw, MONITOR_DEFAULTTONEAREST);
        if (!mon || !GetMonitorInfoW(mon, &mi)) return false;
        RECT r; GetWindowRect(hw, &r);
        return r.left  == mi.rcMonitor.left  && r.top    == mi.rcMonitor.top
            && r.right == mi.rcMonitor.right && r.bottom == mi.rcMonitor.bottom;
    }
}

void init(HWND self) {
    g_self = self;

    apply_exstyle(false);
}

void tick() {
    if (!g_self) return;

    const bool capture_proof_now = lit::features::settings().misc.capture_proof.load();
    if (capture_proof_now != g_last_capture_proof) {
        SetWindowDisplayAffinity(g_self, capture_proof_now ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);
        g_last_capture_proof = capture_proof_now;
    }

    const int panic_vk = lit::features::settings().misc.panic_key.load();
    if (panic_vk && (GetAsyncKeyState(panic_vk) & 1)) {
        ExitProcess(0);
    }

    const int vk = g_menu_key.load();
    if (vk && (GetAsyncKeyState(vk) & 1)) {
        g_visible.store(!g_visible.load());
    }

    using clock = std::chrono::steady_clock;
    const auto now = clock::now();
    if (!g_roblox || (now - g_roblox_last_lookup) > std::chrono::seconds(2)) {
        g_roblox = FindWindowW(nullptr, L"Roblox");
        g_roblox_last_lookup = now;
    }

    HWND fg = GetForegroundWindow();
    if (g_roblox && (fg == g_roblox || fg == g_self)) {
        RECT r{};
        if (GetWindowRect(g_roblox, &r)) {
            int x = r.left, y = r.top;
            int w = r.right - r.left;
            int h = r.bottom - r.top;
            if (is_fullscreen(g_roblox)) {

                w += 16;
            } else {

                x += 4;  y += 31;
                h -= 39;
            }
            if (w > 0 && h > 0) MoveWindow(g_self, x, y, w, h, FALSE);
        }
    } else if (g_roblox) {

        MoveWindow(g_self, 0, 0, 0, 0, FALSE);
    }

    const bool visible_now = g_visible.load();
    if (visible_now != g_last_visible) {
        apply_exstyle(!visible_now);
        if (visible_now) {

            BringWindowToTop(g_self);
            SetForegroundWindow(g_self);
        }
        g_last_visible = visible_now;
    }
}

bool menu_visible()          { return g_visible.load(); }
void set_menu_visible(bool v){ g_visible.store(v); }
int  menu_key()              { return g_menu_key.load(); }
void set_menu_key(int vk)    { g_menu_key.store(vk); }

}
