#include "visualizer/gui/render_settings_panel.hpp"
#include <imgui.h>

namespace gs {

    RenderSettingsPanel::RenderSettingsPanel(std::shared_ptr<RenderingConfig> config)
        : GUIPanel("Rendering Settings"),
          config_(config) {
    }

    void RenderSettingsPanel::render() {
        ImGui::Begin(title_.c_str(), &visible_, window_flags_);
        ImGui::SetWindowSize(ImVec2(300, 0));

        window_active_ = ImGui::IsWindowHovered();

        ImGui::Separator();
        ImGui::Text("Rendering Settings");
        ImGui::Separator();

        renderScaleControl();
        renderFOVControl();
        renderRenderModeInfo();

        ImGui::End();
    }

    void RenderSettingsPanel::renderScaleControl() {
        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("##scale_slider", &config_->scaling_modifier, 0.01f, 3.0f, "Scale=%.2f");
        ImGui::SameLine();
        if (ImGui::Button("Reset##scale", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
            config_->scaling_modifier = 1.0f;
        }
    }

    void RenderSettingsPanel::renderFOVControl() {
        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("##fov_slider", &config_->fov, 45.0f, 120.0f, "FoV=%.2f");
        ImGui::SameLine();
        if (ImGui::Button("Reset##fov", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
            config_->fov = 75.0f;
        }
    }

    void RenderSettingsPanel::renderRenderModeInfo() {
        ImGui::Separator();
        ImGui::Text("Render Info");
        ImGui::Separator();

#ifdef CUDA_GL_INTEROP_ENABLED
        ImGui::Text("Mode: GPU Direct (Interop)");
#else
        ImGui::Text("Mode: CPU Copy");
#endif
    }

} // namespace gs