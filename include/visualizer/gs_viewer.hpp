#pragma once

#include "core/dataset.hpp"
#include "core/trainer.hpp"
#include "visualizer/gui/camera_control_panel.hpp"
#include "visualizer/gui/dataset_viewer_panel.hpp"
#include "visualizer/gui/render_settings_panel.hpp"
#include "visualizer/gui/training_control_panel.hpp"
#include "visualizer/gui/visualization_panel.hpp"
#include "visualizer/viewer_base.hpp"
#include <condition_variable>
#include <memory>
#include <mutex>

namespace gs {

    // Concrete implementation for Gaussian Splatting viewer
    class GSViewer : public ViewerBase {
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

        // Set data sources
        void setTrainer(Trainer* trainer);
        void setDataset(std::shared_ptr<CameraDataset> dataset);

        // ViewerBase overrides
        void onDraw() override;
        void onInitialize() override;
        void onClose() override;
        void setupGUI() override;

        // Public access for synchronization
        std::shared_ptr<TrainingInfo> getTrainingInfo() { return training_info_; }
        std::shared_ptr<Notifier> getNotifier() { return notifier_; }
        std::mutex& getSplatMutex() { return splat_mutex_; }

        // Check if data is available
        bool hasTrainer() const { return trainer_ != nullptr; }
        bool hasDataset() const { return dataset_ != nullptr; }

    private:
        void setupPanels();
        void updateSceneBounds();
        void handleTrainingStart();
        void setupAdditionalKeyBindings();

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
    };

} // namespace gs
