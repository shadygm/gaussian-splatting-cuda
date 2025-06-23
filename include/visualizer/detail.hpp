#pragma once

#include "core/camera.hpp"
#include "core/image_io.hpp"
#include "core/rasterizer.hpp"
#include "core/trainer.hpp"
#include "visualizer/gl_headers.hpp"
#include "visualizer/gui/camera_control_panel.hpp"
#include "visualizer/gui/gui_manager.hpp"
#include "visualizer/gui/render_settings_panel.hpp"
#include "visualizer/gui/training_control_panel.hpp"
#include "visualizer/gui/visualization_panel.hpp"
#include "visualizer/gui/dataset_viewer_panel.hpp"
#include "visualizer/infinite_grid_renderer.hpp"
#include "visualizer/renderer.hpp"
#include "visualizer/shader_manager.hpp"
#include "visualizer/view_cube_renderer.hpp"
#include "visualizer/camera_frustum_renderer.hpp"
#include "visualizer/viewport.hpp"
#include <chrono>
#include <condition_variable>
#include <cuda_runtime.h>
#include <deque>
#include <glm/glm.hpp>
#include <iostream>
#include <memory>
#include <thread>
#include <torch/torch.h>
#include <vector>

using uchar = unsigned char;

namespace gs {

    class ViewerDetail {

    public:
        ViewerDetail(std::string title, int width, int height);

        ~ViewerDetail();

        bool init();

        void updateWindowSize();

        void setFrameRate(const int fps);

        void controlFrameRate();

        static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

        static void cursorPosCallback(GLFWwindow* window, double x, double y);

        static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

        static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

        void run();

        virtual void draw() = 0;

    protected:
        Viewport viewport_;

        // Shader management
        std::unique_ptr<ShaderManager> shader_manager_;

        // GUI management
        std::unique_ptr<GUIManager> gui_manager_;

        // Renderers
        std::shared_ptr<ScreenQuadRenderer> screen_renderer_;

        // Grid renderer
        std::unique_ptr<InfiniteGridRenderer> grid_renderer_;
        bool show_grid_ = true;
        InfiniteGridRenderer::GridPlane grid_plane_ = InfiniteGridRenderer::GridPlane::XZ;

        // View cube renderer
        std::unique_ptr<ViewCubeRenderer> view_cube_renderer_;
        bool show_view_cube_ = true;

        // Camera frustum renderer
        std::unique_ptr<CameraFrustumRenderer> camera_renderer_;

        GLFWwindow* window_;

    private:
        std::string title_;

        static ViewerDetail* detail_;

        int targetFPS = 30;

        int frameTime;

        std::chrono::time_point<std::chrono::high_resolution_clock> lastTime;
    };

    class GSViewer : public ViewerDetail {

        struct Notifier {
        public:
            bool ready = false;
            std::mutex mtx;
            std::condition_variable cv;
        };

    public:
        using TrainingInfo = TrainingControlPanel::TrainingInfo;
        using RenderingConfig = RenderSettingsPanel::RenderingConfig;

        GSViewer(std::string title, int width, int height);
        ~GSViewer();

        void setTrainer(Trainer* trainer);
        void setDataset(std::shared_ptr<CameraDataset> dataset);

        void drawFrame();

        void draw() override;

        // Make this public so ViewerDetail can call it
        void setupGUIPanels();

        // Add accessor to check if trainer is set
        bool hasTrainer() const { return trainer_ != nullptr; }
        bool hasDataset() const { return dataset_ != nullptr; }

    public:
        std::shared_ptr<TrainingInfo> info_;

        std::shared_ptr<Notifier> notifier_;

        std::mutex splat_mtx_;

        // Dataset panel needs to be accessible for key handling
        std::shared_ptr<DatasetViewerPanel> dataset_panel_;

    private:
        void drawGrid();
        void drawViewCube();
        void drawCameras();
        void drawImageOverlay();

        std::shared_ptr<RenderingConfig> config_;

        Trainer* trainer_;
        std::shared_ptr<CameraDataset> dataset_;

        // Store scene bounds for reference (but don't auto-focus on them)
        glm::vec3 scene_center_{0.0f, 0.0f, 0.0f};
        float scene_radius_ = 1.0f;
        bool scene_bounds_valid_ = false;
    };

} // namespace gs