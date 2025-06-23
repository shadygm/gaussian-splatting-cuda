#include "visualizer/gui/visualization_panel.hpp"
#include "visualizer/infinite_grid_renderer.hpp"
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

} // namespace gs