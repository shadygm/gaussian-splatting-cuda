#include "visualizer/gui/visualization_panel.hpp"
#include "visualizer/infinite_grid_renderer.hpp"
#include "visualizer/scene_renderer.hpp"
#include "visualizer/view_cube_renderer.hpp"
#include <imgui.h>

namespace gs {

    VisualizationPanel::VisualizationPanel(
        InfiniteGridRenderer* grid_renderer,
        ViewCubeRenderer* view_cube_renderer,
        bool* show_grid,
        bool* show_view_cube)
        : GUIPanel("Visualization Settings"),
          grid_renderer_(grid_renderer),
          view_cube_renderer_(view_cube_renderer),
          show_grid_(show_grid),
          show_view_cube_(show_view_cube) {
    }

    void VisualizationPanel::render() {
        ImGui::Begin(title_.c_str(), &visible_, window_flags_);
        ImGui::SetWindowSize(ImVec2(300, 0));

        window_active_ = ImGui::IsWindowHovered();

        renderGridSettings();
        renderViewCubeSettings();
        renderGizmoSettings();

        ImGui::End();
    }

    void VisualizationPanel::renderGridSettings() {
        ImGui::Separator();
        ImGui::Text("Grid Settings");
        ImGui::Separator();

        ImGui::Checkbox("Show Grid", show_grid_);

        if (*show_grid_ && grid_renderer_) {
            if (ImGui::SliderFloat("Grid Opacity", &grid_opacity_, 0.0f, 1.0f)) {
                grid_renderer_->setOpacity(grid_opacity_);
            }

            // TODO: Add grid plane selection
            // const char* planes[] = { "YZ", "XZ", "XY" };
            // static int current_plane = 1; // XZ by default
            // if (ImGui::Combo("Grid Plane", &current_plane, planes, 3)) {
            //     grid_plane_ = static_cast<InfiniteGridRenderer::GridPlane>(current_plane);
            // }
        }
    }

    void VisualizationPanel::renderViewCubeSettings() {
        ImGui::Separator();
        ImGui::Text("View Cube");
        ImGui::Separator();

        ImGui::Checkbox("Show View Cube", show_view_cube_);

        if (*show_view_cube_ && view_cube_renderer_) {
            // TODO: Add view cube settings
            // - Size
            // - Position (corner selection)
            // - Opacity
        }
    }

    void VisualizationPanel::renderGizmoSettings() {
        ImGui::Separator();
        ImGui::Text("Transform Gizmos");
        ImGui::Separator();

        if (scene_renderer_) {
            const char* gizmo_modes[] = {"None", "Rotation", "Translation"};
            int current_mode = static_cast<int>(scene_renderer_->getGizmoMode());

            if (ImGui::Combo("Gizmo Mode", &current_mode, gizmo_modes, 3)) {
                scene_renderer_->setGizmoMode(static_cast<SceneRenderer::GizmoMode>(current_mode));
            }

            ImGui::Text("Keyboard shortcuts:");
            ImGui::BulletText("R: Toggle rotation gizmo");
            ImGui::BulletText("T: Toggle translation gizmo");

            auto mode = scene_renderer_->getGizmoMode();
            if (mode == SceneRenderer::GizmoMode::ROTATION) {
                ImGui::Spacing();
                ImGui::Text("Rotation controls:");
                ImGui::BulletText("Red ring: Rotate around X axis");
                ImGui::BulletText("Green ring: Rotate around Y axis");
                ImGui::BulletText("Blue ring: Rotate around Z axis");
            } else if (mode == SceneRenderer::GizmoMode::TRANSLATION) {
                ImGui::Spacing();
                ImGui::Text("Translation controls:");
                ImGui::BulletText("Red arrow: Move along X axis");
                ImGui::BulletText("Green arrow: Move along Y axis");
                ImGui::BulletText("Blue arrow: Move along Z axis");
                ImGui::BulletText("Yellow square: Move in XY plane");
                ImGui::BulletText("Magenta square: Move in XZ plane");
                ImGui::BulletText("Cyan square: Move in YZ plane");
                ImGui::BulletText("Center sphere: Free movement");
            }
        } else {
            ImGui::Text("Scene renderer not available");
        }
    }

} // namespace gs