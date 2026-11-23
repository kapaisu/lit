#include "config.h"
#include "overlay.h"
#include "../features/settings.h"

#include <windows.h>
#include <shlobj.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <imgui.h>
#include <nlohmann/json.hpp>

namespace ui::colors { extern ImVec4 main; }

#pragma comment(lib, "shell32.lib")

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace lit::app::config {

namespace {

fs::path root() {
    wchar_t* raw = nullptr;
    if (!SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &raw)))
        return {};
    fs::path p = raw;
    CoTaskMemFree(raw);
    p /= L"lit";
    std::error_code ec; fs::create_directories(p, ec);
    return p;
}

fs::path autosave_path() {
    fs::path p = root();
    if (p.empty()) return {};
    p /= L"config.json";
    return p;
}

fs::path profiles_dir() {
    fs::path p = root();
    if (p.empty()) return {};
    p /= L"configs";
    std::error_code ec; fs::create_directories(p, ec);
    return p;
}

std::string sanitize(const std::string& name) {
    std::string out;
    out.reserve(name.size());
    for (char c : name) {
        if (isalnum((unsigned char)c) || c == ' ' || c == '-' || c == '_')
            out.push_back(c);
    }
    return out;
}

fs::path profile_path(const std::string& name) {
    const std::string clean = sanitize(name);
    if (clean.empty()) return {};
    fs::path p = profiles_dir();
    if (p.empty()) return {};
    p /= fs::path(clean + ".json");
    return p;
}

json build_json() {
    json j;
    j["menu_key"] = overlay::menu_key();
    j["accent"]   = { ui::colors::main.x, ui::colors::main.y,
                      ui::colors::main.z, ui::colors::main.w };

    auto& e = features::settings().esp;
    j["esp"] = {
        {"enabled",      e.enabled     .load()},
        {"boxes",        e.boxes       .load()},
        {"corner_box",   e.corner_box  .load()},
        {"fill",         e.fill        .load()},
        {"glow",         e.glow        .load()},
        {"rainbow",      e.rainbow     .load()},
        {"color",        e.color       .load()},
        {"health",       e.health      .load()},
        {"names",        e.names       .load()},
        {"distance",     e.distance    .load()},
        {"tracers",      e.tracers     .load()},
        {"team_check",   e.team_check  .load()},
        {"fade_by_distance", e.fade_by_distance.load()},
        {"max_distance", e.max_distance.load()},
    };
    auto& m = features::settings().movement;
    j["movement"] = {
        {"fly",           m.fly          .load()},
        {"noclip",        m.noclip       .load()},
        {"infinite_jump", m.infinite_jump.load()},
        {"walk_speed",    m.walk_speed   .load()},
        {"jump_power",    m.jump_power   .load()},
        {"fly_speed",     m.fly_speed    .load()},
    };
    auto& a = features::settings().aimbot;
    j["aimbot"] = {
        {"enabled",      a.enabled     .load()},
        {"silent",       a.silent      .load()},
        {"sticky",       a.sticky      .load()},
        {"team_check",   a.team_check  .load()},
        {"draw_fov",     a.draw_fov    .load()},
        {"aim_mode",     a.aim_mode    .load()},
        {"aim_key",      a.aim_key     .load()},
        {"hitbox",       a.hitbox      .load()},
        {"fov",          a.fov         .load()},
        {"smoothing",    a.smoothing   .load()},
        {"sensitivity",  a.sensitivity .load()},
        {"prediction",   a.prediction  .load()},
        {"max_distance", a.max_distance.load()},
    };
    auto& c = features::settings().misc;
    j["misc"] = {
        {"anti_afk",      c.anti_afk     .load()},
        {"fullbright",    c.fullbright   .load()},
        {"capture_proof", c.capture_proof.load()},
        {"panic_key",     c.panic_key    .load()},
    };
    return j;
}

void apply_json(const json& j) {
    if (j.contains("menu_key") && j["menu_key"].is_number()) {
        overlay::set_menu_key(j["menu_key"].get<int>());
    }
    if (j.contains("accent") && j["accent"].is_array() && j["accent"].size() == 4) {
        ui::colors::main.x = j["accent"][0].get<float>();
        ui::colors::main.y = j["accent"][1].get<float>();
        ui::colors::main.z = j["accent"][2].get<float>();
        ui::colors::main.w = j["accent"][3].get<float>();
    }
    if (j.contains("esp")) {
        const auto& e = j["esp"];
        auto& s = features::settings().esp;
        if (e.contains("enabled"))      s.enabled     .store(e["enabled"].get<bool>());
        if (e.contains("boxes"))        s.boxes       .store(e["boxes"].get<bool>());
        if (e.contains("corner_box"))   s.corner_box  .store(e["corner_box"].get<bool>());
        if (e.contains("fill"))         s.fill        .store(e["fill"].get<bool>());
        if (e.contains("glow"))         s.glow        .store(e["glow"].get<bool>());
        if (e.contains("rainbow"))      s.rainbow     .store(e["rainbow"].get<bool>());
        if (e.contains("color"))        s.color       .store(e["color"].get<uint32_t>());
        if (e.contains("health"))       s.health      .store(e["health"].get<bool>());
        if (e.contains("names"))        s.names       .store(e["names"].get<bool>());
        if (e.contains("distance"))     s.distance    .store(e["distance"].get<bool>());
        if (e.contains("tracers"))      s.tracers     .store(e["tracers"].get<bool>());
        if (e.contains("team_check"))   s.team_check  .store(e["team_check"].get<bool>());
        if (e.contains("fade_by_distance")) s.fade_by_distance.store(e["fade_by_distance"].get<bool>());
        if (e.contains("max_distance")) s.max_distance.store(e["max_distance"].get<float>());
    }
    if (j.contains("movement")) {
        const auto& m = j["movement"];
        auto& s = features::settings().movement;
        if (m.contains("fly"))          s.fly          .store(m["fly"].get<bool>());
        if (m.contains("noclip"))       s.noclip       .store(m["noclip"].get<bool>());
        if (m.contains("infinite_jump"))s.infinite_jump.store(m["infinite_jump"].get<bool>());
        if (m.contains("walk_speed"))   s.walk_speed   .store(m["walk_speed"].get<float>());
        if (m.contains("jump_power"))   s.jump_power   .store(m["jump_power"].get<float>());
        if (m.contains("fly_speed"))    s.fly_speed    .store(m["fly_speed"].get<float>());
    }
    if (j.contains("aimbot")) {
        const auto& a = j["aimbot"];
        auto& s = features::settings().aimbot;
        if (a.contains("enabled"))      s.enabled     .store(a["enabled"].get<bool>());
        if (a.contains("silent"))       s.silent      .store(a["silent"].get<bool>());
        if (a.contains("sticky"))       s.sticky      .store(a["sticky"].get<bool>());
        if (a.contains("team_check"))   s.team_check  .store(a["team_check"].get<bool>());
        if (a.contains("draw_fov"))     s.draw_fov    .store(a["draw_fov"].get<bool>());
        if (a.contains("aim_mode"))     s.aim_mode    .store(a["aim_mode"].get<int>());
        if (a.contains("aim_key"))      s.aim_key     .store(a["aim_key"].get<int>());
        if (a.contains("hitbox"))       s.hitbox      .store(a["hitbox"].get<int>());
        if (a.contains("fov"))          s.fov         .store(a["fov"].get<float>());
        if (a.contains("smoothing"))    s.smoothing   .store(a["smoothing"].get<float>());
        if (a.contains("sensitivity"))  s.sensitivity .store(a["sensitivity"].get<float>());
        if (a.contains("prediction"))   s.prediction  .store(a["prediction"].get<float>());
        if (a.contains("max_distance")) s.max_distance.store(a["max_distance"].get<float>());
    }
    if (j.contains("misc")) {
        const auto& c = j["misc"];
        auto& s = features::settings().misc;
        if (c.contains("anti_afk"))      s.anti_afk     .store(c["anti_afk"].get<bool>());
        if (c.contains("fullbright"))    s.fullbright   .store(c["fullbright"].get<bool>());
        if (c.contains("capture_proof")) s.capture_proof.store(c["capture_proof"].get<bool>());
        if (c.contains("panic_key"))     s.panic_key    .store(c["panic_key"].get<int>());
    }
}

bool write_json(const fs::path& p, const json& j) {
    if (p.empty()) return false;
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    if (!f.good()) return false;
    const auto out = j.dump(2);
    f.write(out.data(), (std::streamsize)out.size());
    return true;
}

bool read_json(const fs::path& p, json& out) {
    if (p.empty()) return false;
    std::ifstream f(p, std::ios::binary);
    if (!f.good()) return false;
    std::stringstream ss; ss << f.rdbuf();
    try {
        out = json::parse(ss.str());
        return true;
    } catch (...) {

        return false;
    }
}

}

void load() {
    json j;
    if (read_json(autosave_path(), j)) apply_json(j);
}

void save() {
    write_json(autosave_path(), build_json());
}

std::vector<std::string> list_profiles() {
    std::vector<std::string> out;
    const fs::path dir = profiles_dir();
    if (dir.empty()) return out;
    std::error_code ec;
    for (auto& entry : fs::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != L".json") continue;
        out.push_back(entry.path().stem().string());
    }
    std::sort(out.begin(), out.end());
    return out;
}

bool save_profile(const std::string& name) {
    const fs::path p = profile_path(name);
    return write_json(p, build_json());
}

bool load_profile(const std::string& name) {
    const fs::path p = profile_path(name);
    json j;
    if (!read_json(p, j)) return false;
    apply_json(j);
    save();
    return true;
}

bool delete_profile(const std::string& name) {
    const fs::path p = profile_path(name);
    if (p.empty()) return false;
    std::error_code ec;
    return fs::remove(p, ec);
}

}
