#define IMGUI_DEFINE_MATH_OPERATORS
#include "splash.h"
#include "gui.h"
#include "gui_colors.h"

#include "../../../../src/app/state.h"

namespace {
    bool inject_button(const char* id, const char* label, ImVec2 size, bool disabled)
    {
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton(id, size);
        const bool hovered = !disabled && ImGui::IsItemHovered();
        const bool held    = !disabled && ImGui::IsItemActive();
        const bool clicked = !disabled && ImGui::IsItemClicked();

        ImVec4 accent = ui::colors::main;
        if (disabled) {
            accent.w *= 0.35f;
        } else if (held) {
            accent.x *= 0.82f; accent.y *= 0.82f; accent.z *= 0.82f;
        } else if (hovered) {
            accent.x = ImMin(1.f, accent.x * 1.08f);
            accent.y = ImMin(1.f, accent.y * 1.08f);
            accent.z = ImMin(1.f, accent.z * 1.08f);
        }

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 p2(pos.x + size.x, pos.y + size.y);

        dl->AddRectFilled(pos, p2, ImGui::GetColorU32(accent), 6.f);
        dl->AddRect(pos, p2,
            IM_COL32(255, 255, 255, disabled ? 8 : 24),
            6.f, 0, 1.f);

        ImGui::PushFont(font.spacegrotesk_medium[2]);
        const ImVec2 ts = ImGui::CalcTextSize(label);
        const ImVec2 tp(pos.x + (size.x - ts.x) * 0.5f,
                        pos.y + (size.y - ts.y) * 0.5f);

        const float lum = 0.299f * accent.x + 0.587f * accent.y + 0.114f * accent.z;
        const ImU32 tc = lum > 0.55f ? IM_COL32(0, 0, 0, 235) : IM_COL32(255, 255, 255, 235);
        dl->AddText(tp, tc, label);
        ImGui::PopFont();

        return clicked;
    }
}

bool ui::render_splash()
{
    using lit::app::Stage;
    const Stage stage = lit::app::state().stage.load();
    if (stage == Stage::Ready) return false;

    ImGui::SetNextWindowSize(ImVec2(ui::size));
    ImGui::Begin("lit", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoBringToFrontOnFocus);
    {
        ui::render_background();
        ui::render_title();
        ui::render_title_cheat("lit");
        ui::render_build_date();
        ui::render_outline();
        ui::render_watermark();

        const ImVec2 win_pos  = ImGui::GetWindowPos();
        const ImVec2 win_size = ImGui::GetWindowSize();

        const ImVec2 card_sz(360, 220);
        const ImVec2 card_pos(
            win_pos.x + (win_size.x - card_sz.x) * 0.5f,
            win_pos.y + (win_size.y - card_sz.y) * 0.5f);
        const ImVec2 card_end(card_pos.x + card_sz.x, card_pos.y + card_sz.y);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(card_pos, card_end,
            ImGui::GetColorU32(ui::colors::child::top_background),
            ui::rounding);
        dl->AddRect(card_pos, card_end,
            ImGui::GetColorU32(ui::colors::child::outline),
            ui::rounding, 0, 1.f);

        dl->AddRectFilled(
            ImVec2(card_pos.x + 20, card_pos.y + 44),
            ImVec2(card_pos.x + 44, card_pos.y + 46),
            ImGui::GetColorU32(ui::colors::main));

        ImGui::PushFont(font.spacegrotesk_medium[0]);
        dl->AddText(ImVec2(card_pos.x + 20, card_pos.y + 16),
            ImGui::GetColorU32(ui::colors::text), "ready to attach");
        ImGui::PopFont();

        ImGui::PushFont(font.spacegrotesk_medium[1]);
        dl->AddText(ImVec2(card_pos.x + 20, card_pos.y + 56),
            ImGui::GetColorU32(ui::colors::text_disabled),
            "target: RobloxPlayerBeta.exe");
        ImGui::PopFont();

        const bool busy = (stage == Stage::Injecting);
        ImGui::SetCursorScreenPos(ImVec2(card_pos.x + 20, card_pos.y + 96));
        if (inject_button("##inject", busy ? "injecting..." : "inject",
                          ImVec2(card_sz.x - 40, 44), busy))
            lit::app::inject();

        if (busy) {
            const float pct = (float)lit::app::state().progress.load() / 100.f;
            const ImVec2 bar_pos(card_pos.x + 20, card_pos.y + 150);
            const ImVec2 bar_end(card_end.x - 20, bar_pos.y + 3);
            dl->AddRectFilled(bar_pos, bar_end,
                ImGui::GetColorU32(ui::colors::background_dark), 1.5f);
            dl->AddRectFilled(bar_pos,
                ImVec2(bar_pos.x + (bar_end.x - bar_pos.x) * pct, bar_end.y),
                ImGui::GetColorU32(ui::colors::main), 1.5f);
        }

        const std::string& msg = lit::app::state().message;
        if (!msg.empty()) {
            ImGui::PushFont(font.spacegrotesk_medium[1]);
            const ImVec2 msg_pos(card_pos.x + 20, card_end.y - 32);
            dl->AddText(msg_pos,
                ImGui::GetColorU32(ui::colors::text_disabled),
                msg.c_str());
            ImGui::PopFont();
        }
    }
    ImGui::End();
    return true;
}
