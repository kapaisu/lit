#include "gui.h"
#include "gui_colors.h"
#include "../../../../src/features/settings.h"
#include "../../../../src/app/config.h"

void ui::tabs::world(const TabCategory tab)
{
    if (tab.name != "world") return;

    auto& s = lit::features::settings().movement;

    if (ui::begin_child_left("movement", 280))
    {
        bool fly    = s.fly.load();
        bool noclip = s.noclip.load();
        bool ijump  = s.infinite_jump.load();

        if (ImGui::Checkbox("fly (WASD + space/shift)", &fly))    { s.fly.store(fly);              lit::app::config::save(); }
        if (ImGui::Checkbox("noclip",                    &noclip)) { s.noclip.store(noclip);        lit::app::config::save(); }
        if (ImGui::Checkbox("infinite jump",             &ijump))  { s.infinite_jump.store(ijump);  lit::app::config::save(); }
    }
    ImGui::EndChild();

    if (ui::begin_child_right("tuning", 280))
    {
        float ws = s.walk_speed.load();
        float jp = s.jump_power.load();
        float fs = s.fly_speed.load();

        if (ImGui::SliderFloat("walk speed", &ws, 8.f,  500.f, "%.0f")) s.walk_speed.store(ws);
        if (ImGui::IsItemDeactivatedAfterEdit()) lit::app::config::save();

        if (ImGui::SliderFloat("jump power", &jp, 25.f, 500.f, "%.0f")) s.jump_power.store(jp);
        if (ImGui::IsItemDeactivatedAfterEdit()) lit::app::config::save();

        if (ImGui::SliderFloat("fly speed",  &fs, 10.f, 500.f, "%.0f")) s.fly_speed.store(fs);
        if (ImGui::IsItemDeactivatedAfterEdit()) lit::app::config::save();
    }
    ImGui::EndChild();
}
