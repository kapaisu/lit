#include "gui.h"
#include "gui_colors.h"
#include "../../../../src/features/settings.h"
#include "../../../../src/features/scene.h"
#include "../../../../src/roblox/roblox.h"
#include "../../../../src/roblox/offsets.h"
#include "../../../../src/app/overlay.h"
#include "../../../../src/app/config.h"

#include <string>
#include <thread>
#include <vector>

void ui::tabs::settings(const TabCategory tab)
{
    if (tab.name != "settings") return;

    auto& s = lit::features::settings().misc;

    if (ui::begin_child_left("misc", 280))
    {
        bool afk    = s.anti_afk.load();
        bool fb     = s.fullbright.load();
        bool ssp    = s.capture_proof.load();
        int  panic  = s.panic_key.load();

        if (ImGui::Checkbox("anti-afk",   &afk))   s.anti_afk.store(afk);
        if (ImGui::Checkbox("fullbright", &fb))    s.fullbright.store(fb);
        if (ImGui::Checkbox("screen share proof", &ssp)) {
            s.capture_proof.store(ssp);
            lit::app::config::save();
        }

        int menu_key = lit::app::overlay::menu_key();
        if (ImGui::Keybind("menu key",  &menu_key, true)) {
            lit::app::overlay::set_menu_key(menu_key);
            lit::app::config::save();
        }
        if (ImGui::Keybind("panic key", &panic, true)) s.panic_key.store(panic);

        ImGui::Separator();

        ImGui::ColorEdit4("accent", (float*)&ui::colors::main,
            ImGuiColorEditFlags_NoInputs      |
            ImGuiColorEditFlags_NoAlpha       |
            ImGuiColorEditFlags_PickerHueWheel);

        if (ImGui::IsItemDeactivatedAfterEdit()) lit::app::config::save();

        static const ImVec4 presets[] = {
            ImColor(255, 110, 108),
            ImColor(255, 165,  64),
            ImColor(255, 220,  70),
            ImColor(120, 220, 130),
            ImColor( 90, 190, 255),
            ImColor(180, 130, 255),
            ImColor(255, 130, 200),
        };
        for (int i = 0; i < IM_ARRAYSIZE(presets); ++i) {
            ImGui::PushID(i);
            if (i > 0) ImGui::SameLine(0.f, 4.f);
            if (ImGui::ColorButton("##p", presets[i],
                    ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder,
                    ImVec2(22, 22))) {
                ui::colors::main = presets[i];
                lit::app::config::save();
            }
            ImGui::PopID();
        }

        ImGui::Separator();
        ImGui::TextUnformatted("configs");

        static char name_buf[64] = "";
        static std::vector<std::string> profiles;
        static int selected = -1;
        static bool refreshed_once = false;
        if (!refreshed_once) { profiles = lit::app::config::list_profiles(); refreshed_once = true; }

        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##cfgname", "config name...", name_buf, sizeof(name_buf));

        if (ImGui::Button("save", ImVec2(84, 0))) {
            if (name_buf[0] && lit::app::config::save_profile(name_buf)) {
                profiles = lit::app::config::list_profiles();
            }
        }
        ImGui::SameLine();
        const bool have_sel = selected >= 0 && selected < (int)profiles.size();
        ImGui::BeginDisabled(!have_sel);
        if (ImGui::Button("load", ImVec2(84, 0)) && have_sel) {
            lit::app::config::load_profile(profiles[selected]);
        }
        ImGui::SameLine();
        if (ImGui::Button("delete", ImVec2(84, 0)) && have_sel) {
            lit::app::config::delete_profile(profiles[selected]);
            profiles = lit::app::config::list_profiles();
            selected = -1;
        }
        ImGui::EndDisabled();

        if (ImGui::BeginListBox("##cfglist", ImVec2(-1, 90))) {
            for (int i = 0; i < (int)profiles.size(); ++i) {
                const bool is_sel = (i == selected);
                if (ImGui::Selectable(profiles[i].c_str(), is_sel)) selected = i;
            }
            ImGui::EndListBox();
        }
    }
    ImGui::EndChild();

    if (ui::begin_child_right("status", 280))
    {
        const auto& c  = lit::roblox::ctx();
        const auto& os = lit::roblox::offsets::status();

        const char* src = "none";
        switch (os.source) {
            case lit::roblox::offsets::Source::Cache:   src = "cache";   break;
            case lit::roblox::offsets::Source::Network: src = "network"; break;
            default: break;
        }

        ImGui::Text("offsets:  %s (%u)", src, os.count);
        ImGui::Text("version:  %s", os.version.empty() ? "-" : os.version.c_str());
        if (!os.error.empty()) ImGui::Text("error:    %s", os.error.c_str());

        if (ImGui::Button("refresh offsets", ImVec2(-1, 24)))
            std::thread([]{ lit::roblox::offsets::refresh(true); }).detach();

        ImGui::Separator();

        ImGui::Text("process:  %s", c.proc.h ? "attached" : "not attached");
        ImGui::Text("pid:      %u",  c.proc.pid);
        ImGui::Text("base:     0x%llX", (unsigned long long)c.proc.base);
        ImGui::Text("ready:    %s", c.ready.load() ? "yes" : "no");
        ImGui::Text("dm:       0x%llX", (unsigned long long)c.data_model);
        ImGui::Text("wspace:   0x%llX", (unsigned long long)c.workspace);
        ImGui::Text("camera:   0x%llX", (unsigned long long)c.camera);
        ImGui::Text("players:  0x%llX", (unsigned long long)c.players);
        ImGui::Text("localplr: 0x%llX", (unsigned long long)c.local_player);

        ImGui::Separator();

        std::vector<lit::features::Entity> ents;
        lit::features::Camera cam{};
        lit::features::scene().read(ents, cam);
        ImGui::Text("entities: %zu", ents.size());
        ImGui::Text("cam dims: %.0f x %.0f", cam.width, cam.height);
        ImGui::Text("vp[0..3]: %.2f %.2f %.2f %.2f",
            cam.vp[0], cam.vp[1], cam.vp[2], cam.vp[3]);
        if (!ents.empty()) {
            const auto& e = ents.front();
            ImGui::Text("first: %s hp=%.0f/%0.f pos=%.0f,%.0f,%.0f",
                e.name.empty() ? "-" : e.name.c_str(),
                e.health, e.max_health, e.x, e.y, e.z);
        }
    }
    ImGui::EndChild();
}
