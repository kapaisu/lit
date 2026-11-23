#include "roblox.h"
#include "offsets.h"
#include "instance.h"
#include "../core/memory.h"

namespace lit::roblox {

Context& ctx() {
    static Context c;
    return c;
}

namespace {

std::string version_from_image_path(const std::wstring& path) {
    const auto sep = path.find_last_of(L"\\/");
    if (sep == std::wstring::npos) return {};
    const auto dir_end = sep;
    const auto dir_sep = path.find_last_of(L"\\/", dir_end - 1);
    const std::wstring dir = (dir_sep == std::wstring::npos)
        ? path.substr(0, dir_end)
        : path.substr(dir_sep + 1, dir_end - dir_sep - 1);
    if (dir.rfind(L"version-", 0) != 0) return {};
    std::string out(dir.begin(), dir.end());
    return out;
}

}

bool attach() {
    auto& c = ctx();
    if (c.proc.h) return true;

    uint32_t pid = process::find_pid(L"RobloxPlayerBeta.exe");
    if (!pid) return false;
    if (!process::open(c.proc, pid)) return false;

    uintptr_t base = 0; uint32_t size = 0;
    if (!process::module_bounds(c.proc, L"RobloxPlayerBeta.exe", base, size)) {
        process::close(c.proc);
        return false;
    }
    c.proc.base = base;
    c.proc.size = size;
    c.ready.store(false);

    const std::string real_version = version_from_image_path(process::image_path(c.proc));
    if (!real_version.empty())
        offsets::refresh(false, real_version);

    refresh_pointers();
    return true;
}

void refresh_pointers() {
    auto& c = ctx();
    if (!c.proc.h) return;

    const uint32_t o_visual = offsets::get("VisualEngine", "Pointer");
    if (!o_visual) return;

    const uintptr_t visual_engine =
        memory::read<uintptr_t>(c.proc, c.proc.base + o_visual);
    if (!visual_engine) return;

    const uint32_t o_fake = offsets::get("VisualEngine", "FakeDataModel");
    const uintptr_t fake_dm = o_fake
        ? memory::read<uintptr_t>(c.proc, visual_engine + o_fake)
        : 0;

    const uint32_t o_real = offsets::get("FakeDataModel", "RealDataModel");
    c.data_model = (fake_dm && o_real)
        ? memory::read<uintptr_t>(c.proc, fake_dm + o_real)
        : 0;

    if (!c.data_model) {
        c.workspace = c.players = c.local_player = c.camera = 0;
        c.ready.store(false);
        return;
    }

    const uint32_t o_ws     = offsets::get("DataModel", "Workspace");
    const uint32_t o_cam    = offsets::get("Workspace", "CurrentCamera");
    c.workspace = o_ws ? memory::read<uintptr_t>(c.proc, c.data_model + o_ws) : 0;
    c.camera    = (c.workspace && o_cam)
        ? memory::read<uintptr_t>(c.proc, c.workspace + o_cam)
        : 0;

    if (!c.players)
        c.players = instance::find_child_by_class(c.data_model, "Players");
    if (!c.players)
        c.players = instance::find_child_by_name (c.data_model, "Players");

    if (!c.mouse_service)
        c.mouse_service = instance::find_child_by_class(c.data_model, "MouseService");
    if (!c.mouse_service)
        c.mouse_service = instance::find_child_by_name (c.data_model, "MouseService");

    const uint32_t o_local = offsets::get("Player", "LocalPlayer");
    c.local_player = (c.players && o_local)
        ? memory::read<uintptr_t>(c.proc, c.players + o_local)
        : 0;

    c.ready.store(c.data_model && c.workspace);
}

void detach() {
    auto& c = ctx();
    process::close(c.proc);
    c.data_model = c.workspace = c.players = c.local_player = c.camera = 0;
    c.mouse_service = 0;
    c.ready.store(false);
}

}
