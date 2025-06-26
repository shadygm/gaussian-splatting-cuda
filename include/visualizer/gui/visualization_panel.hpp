#pragma once

#include "visualizer/gui/gui_manager.hpp"

namespace gs {

    // Forward declarations
    class InfiniteGridRenderer;
    class ViewCubeRenderer;
    class SceneRenderer;

    class VisualizationPanel : public GUIPanel {
    public:
        VisualizationPanel(
            InfiniteGridRenderer* grid_renderer,
            ViewCubeRenderer* view_cube_renderer,
            bool* show_grid,
            bool* show_view_cube);

        void render() override;

        // Set scene renderer for gizmo control
        void setSceneRenderer(SceneRenderer* renderer) { scene_renderer_ = renderer; }

    private:
        void renderGridSettings();
        void renderViewCubeSettings();
        void renderGizmoSettings();

        InfiniteGridRenderer* grid_renderer_;
        ViewCubeRenderer* view_cube_renderer_;
        SceneRenderer* scene_renderer_ = nullptr;
        bool* show_grid_;
        bool* show_view_cube_;

        float grid_opacity_ = 1.0f;
    };

} // namespace gs