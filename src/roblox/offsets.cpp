#include "offsets.h"
#include "../core/net.h"

#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <unordered_map>

#include <nlohmann/json.hpp>

#pragma comment(lib, "shell32.lib")

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace lit::roblox::offsets {

namespace {
    std::mutex g_mtx;
    Status     g_status;
    std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>> g_map;

    std::wstring appdata_path() {
        wchar_t* raw = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &raw))) {
            std::wstring p = raw;
            CoTaskMemFree(raw);
            return p;
        }
        return L"";
    }

    std::wstring localappdata_path() {
        wchar_t* raw = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &raw))) {
            std::wstring p = raw;
            CoTaskMemFree(raw);
            return p;
        }
        return L"";
    }

    fs::path cache_path_for(const std::string& version) {
        fs::path base = appdata_path();
        if (base.empty()) return {};
        base /= L"lit";
        base /= L"offsets";
        std::error_code ec;
        fs::create_directories(base, ec);
        base /= (version + ".json");
        return base;
    }

    bool parse_json(const std::string& body, std::string& version_out, uint32_t& count_out) {
        try {
            auto j = json::parse(body);
            std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>> next;
            uint32_t total = 0;
            const auto& tbl = j.contains("Offsets") ? j["Offsets"] : j;
            for (auto it = tbl.begin(); it != tbl.end(); ++it) {
                if (!it->is_object()) continue;
                auto& inner = next[it.key()];
                for (auto f = it->begin(); f != it->end(); ++f) {
                    if (f->is_number()) {
                        inner[f.key()] = f->get<uint32_t>();
                        ++total;
                    }
                }
            }
            std::string ver;
            if (j.contains("Roblox Version") && j["Roblox Version"].is_string())
                ver = j["Roblox Version"].get<std::string>();

            std::lock_guard lk(g_mtx);
            g_map = std::move(next);
            version_out = ver;
            count_out = total;
            return true;
        } catch (...) {
            return false;
        }
    }
}

std::string detect_roblox_version() {
    fs::path base = localappdata_path();
    if (base.empty()) return {};
    base /= L"Roblox";
    base /= L"Versions";

    std::error_code ec;
    if (!fs::exists(base, ec)) return {};

    fs::file_time_type newest{};
    std::string chosen;

    for (auto& e : fs::directory_iterator(base, ec)) {
        if (!e.is_directory()) continue;
        const auto name = e.path().filename().string();
        if (name.rfind("version-", 0) != 0) continue;
        if (!fs::exists(e.path() / L"RobloxPlayerBeta.exe", ec)) continue;

        auto t = fs::last_write_time(e.path(), ec);
        if (chosen.empty() || t > newest) {
            newest = t;
            chosen = name;
        }
    }
    return chosen;
}

void refresh(bool force_network, std::string_view explicit_version) {

    static constexpr const char* kPinnedVersion = "version-f5a60436d48947d3";

    std::string version(explicit_version);
    if (version.empty()) version = detect_roblox_version();
    if (version.empty()) version = kPinnedVersion;

    {
        std::lock_guard lk(g_mtx);
        g_status = Status{};
        g_status.version = version;
    }

    const fs::path cache = cache_path_for(version);

    if (!force_network && !cache.empty()) {
        std::ifstream f(cache, std::ios::binary);
        if (f.good()) {
            std::stringstream ss; ss << f.rdbuf();
            std::string ver_out;
            uint32_t count_out = 0;
            if (parse_json(ss.str(), ver_out, count_out) && count_out > 0) {
                std::lock_guard lk(g_mtx);
                g_status.source = Source::Cache;
                g_status.count  = count_out;
                if (!ver_out.empty()) g_status.version = ver_out;
                return;
            }
        }
    }

    const std::wstring url =
        L"https://offsets.imtheo.lol/" +
        std::wstring(version.begin(), version.end()) +
        L"/offsets.json";

    std::string body = net::http_get(url);
    if (body.empty()) {
        std::lock_guard lk(g_mtx);
        g_status.error = "network fetch failed";
        return;
    }

    std::string ver_out;
    uint32_t count_out = 0;
    if (!parse_json(body, ver_out, count_out) || count_out == 0) {
        std::lock_guard lk(g_mtx);
        g_status.error = "JSON parse failed";
        return;
    }

    if (!cache.empty()) {
        std::ofstream f(cache, std::ios::binary | std::ios::trunc);
        if (f.good()) f.write(body.data(), (std::streamsize)body.size());
    }

    std::lock_guard lk(g_mtx);
    g_status.source = Source::Network;
    g_status.count  = count_out;
    if (!ver_out.empty()) g_status.version = ver_out;
}

uint32_t get(std::string_view klass, std::string_view field) {
    std::lock_guard lk(g_mtx);
    auto it = g_map.find(std::string(klass));
    if (it == g_map.end()) return 0;
    auto jt = it->second.find(std::string(field));
    return jt == it->second.end() ? 0u : jt->second;
}

uint32_t get_or(std::string_view klass, std::string_view field, uint32_t fallback) {
    uint32_t v = get(klass, field);
    return v ? v : fallback;
}

const Status& status() {
    std::lock_guard lk(g_mtx);
    return g_status;
}

}
