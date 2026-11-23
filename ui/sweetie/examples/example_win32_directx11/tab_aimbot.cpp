#include "gui.h"
#include "gui_colors.h"
#include "../../../../src/features/settings.h"
#include "../../../../src/app/config.h"

void ui::tabs::aimbot(const TabCategory tab)
{
    if (tab.name != "aimbot") return;

    auto& s = lit::features::settings().aimbot;

    if (ui::begin_child_left("main", 280))
    {
        bool enabled    = s.enabled.load();
        bool silent     = s.silent.load();
        bool sticky     = s.sticky.load();
        bool team_check = s.team_check.load();
        bool draw_fov   = s.draw_fov.load();
        int  key        = s.aim_key.load();
        int  hitbox     = s.hitbox.load();
        int  mode       = s.aim_mode.load();

        if (ImGui::Checkbox("enabled",     &enabled))    { s.enabled.store(enabled);       lit::app::config::save(); }
        if (ImGui::Checkbox("silent aim",  &silent))     { s.silent.store(silent);         lit::app::config::save(); }
        if (ImGui::Checkbox("sticky lock", &sticky))     { s.sticky.store(sticky);         lit::app::config::save(); }
        if (ImGui::Checkbox("team check",  &team_check)) { s.team_check.store(team_check); lit::app::config::save(); }
        if (ImGui::Checkbox("draw fov",    &draw_fov))   { s.draw_fov.store(draw_fov);     lit::app::config::save(); }

        const char* modes[] = { "camera write", "cursor move" };
        if (ImGui::Combo("aim mode", &mode, modes, IM_ARRAYSIZE(modes))) {
            s.aim_mode.store(mode);
            lit::app::config::save();
        }

        const char* hitboxes[] = { "head", "neck", "upper torso", "lower torso", "root part" };
        if (ImGui::Combo("hitbox", &hitbox, hitboxes, IM_ARRAYSIZE(hitboxes))) {
            s.hitbox.store(hitbox);
            lit::app::config::save();
        }

        if (ImGui::Keybind("aim key", &key, true)) {
            s.aim_key.store(key);
            lit::app::config::save();
        }
    }
    ImGui::EndChild();

    if (ui::begin_child_right("tuning", 280))
    {
        float fov         = s.fov.load();
        float smoothing   = s.smoothing.load();
        float sensitivity = s.sensitivity.load();
        float prediction  = s.prediction.load();
        float distance    = s.max_distance.load();

        if (ImGui::SliderFloat("fov (px)",     &fov,         0.f, 800.f,   "%.0f"))   s.fov.store(fov);
        if (ImGui::IsItemDeactivatedAfterEdit()) lit::app::config::save();

        if (ImGui::SliderFloat("smoothing",    &smoothing,   1.f, 30.f,    "%.1f"))   s.smoothing.store(smoothing);
        if (ImGui::IsItemDeactivatedAfterEdit()) lit::app::config::save();

        if (ImGui::SliderFloat("sensitivity",  &sensitivity, 0.1f, 30.f,   "%.1f"))   s.sensitivity.store(sensitivity);
        if (ImGui::IsItemDeactivatedAfterEdit()) lit::app::config::save();

        if (ImGui::SliderFloat("prediction",   &prediction,  0.f, 0.5f,    "%.2f s")) s.prediction.store(prediction);
        if (ImGui::IsItemDeactivatedAfterEdit()) lit::app::config::save();

        if (ImGui::SliderFloat("max distance", &distance,    50.f, 10000.f, "%.0f"))  s.max_distance.store(distance);
        if (ImGui::IsItemDeactivatedAfterEdit()) lit::app::config::save();
    }
    ImGui::EndChild();
}
