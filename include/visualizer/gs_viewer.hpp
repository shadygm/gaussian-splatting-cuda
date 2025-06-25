#pragma once

#include "core/dataset.hpp"
#include "core/trainer.hpp"
#include "visualizer/gl_headers.hpp"
#include "visualizer/gui/camera_control_panel.hpp"
#include "visualizer/gui/dataset_viewer_panel.hpp"
#include "visualizer/gui/gui_manager.hpp"
#include "visualizer/gui/render_settings_panel.hpp"
#include "visualizer/gui/training_control_panel.hpp"
#include "visualizer/gui/visualization_panel.hpp"
#include "visualizer/input_handler.hpp"
#include "visualizer/scene_renderer.hpp"
#include "visualizer/viewport.hpp"
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>

namespace gs {

    // Concrete implementation for Gaussian Splatting viewer
    class GSViewer {
    public:
        // Synchronization structures
        struct Notifier {
            bool ready = false;
            std::mutex mtx;
            std::condition_variable cv;
        };

        using TrainingInfo = TrainingControlPanel::TrainingInfo;
        using RenderingConfig = RenderSettingsPanel::RenderingConfig;

        GSViewer(const std::string& title, int width, int height);
        ~GSViewer();

        // Main run loop
        void run();

        // Set data sources
        void setTrainer(Trainer* trainer);
        void setDataset(std::shared_ptr<CameraDataset> dataset);

        // Virtual methods for customization
        virtual void onDraw();
        virtual void onInitialize();
        virtual void onResize(int width, int height) {}
        virtual void onClose();
        virtual void setupGUI();

        // Access to components
        Viewport& getViewport() { return viewport_; }
        SceneRenderer* getSceneRenderer() { return scene_renderer_.get(); }
        GUIManager* getGUIManager() { return gui_manager_.get(); }
        InputHandler* getInputHandler() { return input_handler_.get(); }

        // Window access
        GLFWwindow* getWindow() { return window_; }

        // Public access for synchronization
        std::shared_ptr<TrainingInfo> getTrainingInfo() { return training_info_; }
        std::shared_ptr<Notifier> getNotifier() { return notifier_; }
        std::mutex& getSplatMutex() { return splat_mutex_; }

        // Check if data is available
        bool hasTrainer() const { return trainer_ != nullptr; }
        bool hasDataset() const { return dataset_ != nullptr; }

    protected:
        // Window and context initialization
        bool initializeWindow();
        bool initializeOpenGL();
        bool initializeComponents();
        void shutdownWindow();

        // Frame control
        void setTargetFPS(int fps);
        void limitFrameRate();

        // Window management
        void updateWindowSize();

        GLFWwindow* window_ = nullptr;
        std::string title_;
        Viewport viewport_;

        // Components
        std::unique_ptr<SceneRenderer> scene_renderer_;
        std::unique_ptr<GUIManager> gui_manager_;
        std::unique_ptr<InputHandler> input_handler_;

        // Frame timing
        int target_fps_ = 30;
        std::chrono::steady_clock::time_point last_frame_time_;

        // Paths
        std::string shader_path_;

        // Flags
        bool initialized_ = false;

    private:
        void setupPanels();
        void updateSceneBounds();
        void handleTrainingStart();
        void setupAdditionalKeyBindings();
        void drawHelpOverlay();

        // Data sources
        Trainer* trainer_ = nullptr;
        std::shared_ptr<CameraDataset> dataset_;

        // Synchronization
        std::shared_ptr<TrainingInfo> training_info_;
        std::shared_ptr<Notifier> notifier_;
        std::mutex splat_mutex_;

        // Rendering
        SceneRenderer::RenderSettings render_settings_;
        std::shared_ptr<RenderingConfig> render_config_;

        // Scene info
        glm::vec3 scene_center_{0.0f};
        float scene_radius_{1.0f};
        bool scene_bounds_valid_ = false;
        bool scene_bounds_initialized_ = false;

        // GUI panels
        std::shared_ptr<TrainingControlPanel> training_panel_;
        std::shared_ptr<RenderSettingsPanel> render_panel_;
        std::shared_ptr<CameraControlPanel> camera_panel_;
        std::shared_ptr<VisualizationPanel> viz_panel_;
        std::shared_ptr<DatasetViewerPanel> dataset_panel_;

        // Help overlay
        bool show_help_ = false;
    };

} // namespace gs