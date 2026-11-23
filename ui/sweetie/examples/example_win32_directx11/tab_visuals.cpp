#define IMGUI_DEFINE_MATH_OPERATORS
#include "gui.h"
#include "gui_colors.h"
#include "../../../../src/features/settings.h"
#include "../../../../src/features/preview.h"
#include "../../../../src/features/esp.h"
#include "../../../../src/app/config.h"

#include <d3d11.h>

static void draw_esp_preview(ImVec2 pane_min, ImVec2 pane_max)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const auto& s = lit::features::settings().esp;

    dl->AddRectFilled(pane_min, pane_max,
        ImGui::GetColorU32(ui::colors::child::top_background), ui::rounding);
    dl->AddRect(pane_min, pane_max,
        ImGui::GetColorU32(ui::colors::child::outline),
        ui::rounding, 0, 1.f);

    const uint32_t rw = (uint32_t)(pane_max.x - pane_min.x);
    const uint32_t rh = (uint32_t)(pane_max.y - pane_min.y);
    ID3D11ShaderResourceView* srv = lit::features::preview::render(
        ImGui::GetIO().DeltaTime, rw, rh);
    if (srv) {
        dl->AddImage(srv, pane_min, pane_max);
    }

    const float cx = (pane_min.x + pane_max.x) * 0.5f;
    const float pane_h = pane_max.y - pane_min.y;
    const float h  = pane_h * 0.72f;
    const float w  = h * 0.30f;
    const float y_center = (pane_min.y + pane_max.y) * 0.5f + pane_h * 0.03f;
    const ImVec2 tl(cx - w * 0.5f, y_center - h * 0.5f);
    const ImVec2 br(cx + w * 0.5f, y_center + h * 0.5f);

    const bool rainbow = s.rainbow.load();
    const uint32_t base_col = rainbow
        ? lit::features::esp::rainbow_color(ImGui::GetTime())
        : s.color.load();

    if (s.glow.load())
        lit::features::esp::draw_glow(dl, tl.x, tl.y, br.x, br.y, base_col, 1.f);

    if (s.fill.load()) {
        dl->AddRectFilled(tl, br, lit::features::esp::with_alpha(base_col, 0.18f), 2.f);
    }

    if (s.boxes.load()) {
        if (s.corner_box.load()) {
            lit::features::esp::draw_corner_box(dl, tl.x, tl.y, br.x, br.y, IM_COL32(0, 0, 0, 220), 4.f);
            lit::features::esp::draw_corner_box(dl, tl.x, tl.y, br.x, br.y, base_col, 2.f);
        } else {
            dl->AddRect(tl, br, IM_COL32(0, 0, 0, 220), 0.f, 0, 3.f);
            dl->AddRect(tl, br, base_col, 0.f, 0, 1.f);
        }
    }
    if (s.health.load()) {
        const ImVec2 bar_tl(tl.x - 6.f, tl.y);
        const ImVec2 bar_br(tl.x - 2.f, br.y);
        dl->AddRectFilled(bar_tl, bar_br, IM_COL32(0, 0, 0, 200));

        const float pct = 0.68f;
        const float fill_h = (bar_br.y - bar_tl.y) * pct;
        dl->AddRectFilled(
            ImVec2(bar_tl.x, bar_br.y - fill_h),
            bar_br,
            IM_COL32(int((1.f - pct) * 2.f * 255), 255, 0, 255));
    }
    if (s.names.load()) {
        const char* n = "player1";
        const ImVec2 ts = ImGui::CalcTextSize(n);
        const ImVec2 tp(cx - ts.x * 0.5f, tl.y - ts.y - 2.f);
        dl->AddText(ImVec2(tp.x + 1, tp.y + 1), IM_COL32(0, 0, 0, 220), n);
        dl->AddText(tp, IM_COL32(255, 255, 255, 235), n);
    }
    if (s.distance.load()) {
        const char* d = "42m";
        const ImVec2 ts = ImGui::CalcTextSize(d);
        const ImVec2 tp(cx - ts.x * 0.5f, br.y + 2.f);
        dl->AddText(ImVec2(tp.x + 1, tp.y + 1), IM_COL32(0, 0, 0, 220), d);
        dl->AddText(tp, IM_COL32(200, 200, 210, 235), d);
    }
    if (s.tracers.load()) {
        dl->AddLine(ImVec2(cx, pane_max.y - 4.f),
                    ImVec2(cx, br.y),
                    lit::features::esp::with_alpha(base_col, 0.75f), 1.f);
    }

    dl->AddText(ImVec2(pane_min.x + 10, pane_min.y + 8),
        ImGui::GetColorU32(ui::colors::text_disabled), "preview");
}

void ui::tabs::visuals(const TabCategory tab)
{
    if (tab.name != "visuals") return;

    auto& s = lit::features::settings().esp;

    if (ui::begin_child_left("esp", 280))
    {
        bool enabled    = s.enabled.load();
        bool boxes      = s.boxes.load();
        bool corner_box = s.corner_box.load();
        bool fill       = s.fill.load();
        bool glow       = s.glow.load();
        bool rainbow    = s.rainbow.load();
        bool health     = s.health.load();
        bool names      = s.names.load();
        bool distance   = s.distance.load();
        bool tracers    = s.tracers.load();
        bool team_check = s.team_check.load();
        bool fade       = s.fade_by_distance.load();

        if (ImGui::Checkbox("esp enabled", &enabled))    { s.enabled.store(enabled);       lit::app::config::save(); }
        if (ImGui::Checkbox("boxes",       &boxes))      { s.boxes.store(boxes);           lit::app::config::save(); }
        if (ImGui::Checkbox("corner box",  &corner_box)) { s.corner_box.store(corner_box); lit::app::config::save(); }
        if (ImGui::Checkbox("fill chams",  &fill))       { s.fill.store(fill);             lit::app::config::save(); }
        if (ImGui::Checkbox("glow chams",  &glow))       { s.glow.store(glow);             lit::app::config::save(); }
        if (ImGui::Checkbox("rainbow",     &rainbow))    { s.rainbow.store(rainbow);       lit::app::config::save(); }
        if (ImGui::Checkbox("health bar",  &health))     { s.health.store(health);         lit::app::config::save(); }
        if (ImGui::Checkbox("names",       &names))      { s.names.store(names);           lit::app::config::save(); }
        if (ImGui::Checkbox("distance",    &distance))   { s.distance.store(distance);     lit::app::config::save(); }
        if (ImGui::Checkbox("tracers",     &tracers))    { s.tracers.store(tracers);       lit::app::config::save(); }
        if (ImGui::Checkbox("team check",  &team_check)) { s.team_check.store(team_check); lit::app::config::save(); }
        if (ImGui::Checkbox("fade by distance", &fade))  { s.fade_by_distance.store(fade); lit::app::config::save(); }

        ImGui::BeginDisabled(rainbow);
        const ImVec4 cur_col = ImGui::ColorConvertU32ToFloat4(s.color.load());
        float col4[4] = { cur_col.x, cur_col.y, cur_col.z, cur_col.w };
        if (ImGui::ColorEdit4("esp color", col4,
                ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueWheel)) {
            s.color.store(ImGui::ColorConvertFloat4ToU32(ImVec4(col4[0], col4[1], col4[2], col4[3])));
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) lit::app::config::save();
        ImGui::EndDisabled();
    }
    ImGui::EndChild();

    if (ui::begin_child_right("preview", 280))
    {
        float d = s.max_distance.load();
        if (ImGui::SliderFloat("render distance", &d, 100.f, 10000.f, "%.0f"))
            s.max_distance.store(d);
        if (ImGui::IsItemDeactivatedAfterEdit()) lit::app::config::save();

        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const ImVec2 cur = ImGui::GetCursorScreenPos();
        if (avail.y > 40.f) {
            draw_esp_preview(ImVec2(cur.x, cur.y + 6),
                             ImVec2(cur.x + avail.x, cur.y + avail.y));
        }
    }
    ImGui::EndChild();
}
