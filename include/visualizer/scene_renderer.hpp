#pragma once

#include "core/dataset.hpp"
#include "core/trainer.hpp"
#include "visualizer/camera_frustum_renderer.hpp"
#include "visualizer/gui/render_settings_panel.hpp"
#include "visualizer/infinite_grid_renderer.hpp"
#include "visualizer/opengl_state_manager.hpp"
#include "visualizer/renderer.hpp"
#include "visualizer/shader_manager.hpp"
#include "visualizer/view_cube_renderer.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <torch/torch.h>

namespace gs {

    // Encapsulates all scene rendering logic
    class SceneRenderer {
    public:
        struct RenderSettings {
            bool show_grid;
            bool show_view_cube;
            bool show_cameras;
            bool show_image_overlay;
            InfiniteGridRenderer::GridPlane grid_plane;

            RenderSettings()
                : show_grid(true),
                  show_view_cube(true),
                  show_cameras(true),
                  show_image_overlay(false),
                  grid_plane(InfiniteGridRenderer::GridPlane::XZ) {}
        };

        SceneRenderer();
        ~SceneRenderer();

        // Initialize all renderers
        bool initialize(const std::string& shader_path);

        void renderSplats(const Viewport& viewport,
                          Trainer* trainer,
                          std::shared_ptr<RenderSettingsPanel::RenderingConfig> config,
                          std::mutex& splat_mutex);

        // Individual component renders
        void renderGrid(const Viewport& viewport, const RenderSettings& settings);
        void renderViewCube(const Viewport& viewport, bool show);
        void renderCameras(const Viewport& viewport, int highlight_index = -1);
        void renderImageOverlay(const Viewport& viewport,
                                const torch::Tensor& image,
                                float x, float y, float width, float height);

        // Scene management
        void updateSceneBounds(const glm::vec3& center, float radius);
        void setCameras(const std::vector<std::shared_ptr<Camera>>& cameras,
                        const std::vector<bool>& is_test_camera);

        // View cube interaction
        int hitTestViewCube(const Viewport& viewport, float screen_x, float screen_y);

        // Getters for GUI interaction
        InfiniteGridRenderer* getGridRenderer() { return grid_renderer_.get(); }
        ViewCubeRenderer* getViewCubeRenderer() { return view_cube_renderer_.get(); }
        CameraFrustumRenderer* getCameraRenderer() { return camera_renderer_.get(); }
        ShaderManager* getShaderManager() { return shader_manager_.get(); }
        std::shared_ptr<ScreenQuadRenderer> getScreenRenderer() { return screen_renderer_; }

    private:
        // Renderers
        std::unique_ptr<ShaderManager> shader_manager_;
        std::unique_ptr<InfiniteGridRenderer> grid_renderer_;
        std::unique_ptr<ViewCubeRenderer> view_cube_renderer_;
        std::unique_ptr<CameraFrustumRenderer> camera_renderer_;
        std::shared_ptr<ScreenQuadRenderer> screen_renderer_;

        // Scene info
        glm::vec3 scene_center_{0.0f};
        float scene_radius_{1.0f};
        bool scene_bounds_valid_ = false;

        // View cube position
        float view_cube_margin_ = 20.0f;
        float view_cube_size_ = 120.0f;

        bool initialized_ = false;
    };

} // namespace gs