#include "visualizer/gui/camera_control_panel.hpp"
#include <imgui.h>

namespace gs {

    CameraControlPanel::CameraControlPanel(Viewport* viewport)
        : GUIPanel("Camera Controls"),
          viewport_(viewport) {
    }

    void CameraControlPanel::render() {
        ImGui::Begin(title_.c_str(), &visible_, window_flags_);
        ImGui::SetWindowSize(ImVec2(300, 0));

        window_active_ = ImGui::IsWindowHovered();

        renderCameraControls();
        renderCameraInfo();
        renderQuickViewButtons();
        renderSceneInfo();

        ImGui::End();
    }

    void CameraControlPanel::renderCameraControls() {
        ImGui::Separator();
        ImGui::Text("Camera Controls");
        ImGui::Separator();

        ImGui::Text("Left Mouse: Orbit");
        ImGui::Text("Right Mouse: Pan");
        ImGui::Text("Scroll: Zoom");
        ImGui::Text("G: Toggle Grid");
        ImGui::Text("F: Focus World Origin");
        ImGui::Text("H: Home View (Look Down)");

        if (ImGui::Button("Reset Camera", ImVec2(-1, 0))) {
            viewport_->reset();
        }
    }

    void CameraControlPanel::renderCameraInfo() {
        ImGui::Separator();
        ImGui::Text("Camera Parameters");
        ImGui::Separator();

        ImGui::Text("Distance: %.2f", viewport_->distance);
        ImGui::Text("Azimuth: %.1f°", viewport_->azimuth);
        ImGui::Text("Elevation: %.1f°", viewport_->elevation);
        ImGui::Text("Target: %.2f, %.2f, %.2f",
                    viewport_->target.x,
                    viewport_->target.y,
                    viewport_->target.z);
    }

    void CameraControlPanel::renderQuickViewButtons() {
        ImGui::Separator();
        ImGui::Text("Quick Views:");

        if (ImGui::Button("Front", ImVec2(60, 0)))
            viewport_->alignToAxis('z', true);
        ImGui::SameLine();
        if (ImGui::Button("Back", ImVec2(60, 0)))
            viewport_->alignToAxis('z', false);
        ImGui::SameLine();
        if (ImGui::Button("Left", ImVec2(60, 0)))
            viewport_->alignToAxis('x', false);
        ImGui::SameLine();
        if (ImGui::Button("Right", ImVec2(60, 0)))
            viewport_->alignToAxis('x', true);

        if (ImGui::Button("Top", ImVec2(60, 0)))
            viewport_->alignToAxis('y', true);
        ImGui::SameLine();
        if (ImGui::Button("Bottom", ImVec2(60, 0)))
            viewport_->alignToAxis('y', false);
    }

    void CameraControlPanel::renderSceneInfo() {
        if (scene_bounds_valid_) {
            ImGui::Separator();
            ImGui::Text("Scene Info:");
            ImGui::Text("Center: %.2f, %.2f, %.2f",
                        scene_center_.x, scene_center_.y, scene_center_.z);
            ImGui::Text("Radius: %.2f", scene_radius_);
        }
    }

    void CameraControlPanel::setSceneBounds(const glm::vec3& center, float radius) {
        scene_center_ = center;
        scene_radius_ = radius;
        scene_bounds_valid_ = true;
    }

} // namespace gs