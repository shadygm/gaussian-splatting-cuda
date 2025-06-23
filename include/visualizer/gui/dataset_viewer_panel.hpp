#pragma once

#include "visualizer/gui/gui_manager.hpp"
#include "core/dataset.hpp"
#include <memory>

namespace gs {

    // Forward declarations
    class CameraFrustumRenderer;
}

// Viewport is not in gs namespace
class Viewport;

namespace gs {

    class DatasetViewerPanel : public GUIPanel {
    public:
        DatasetViewerPanel(
            std::shared_ptr<CameraDataset> dataset,
            CameraFrustumRenderer* frustum_renderer,
            Viewport* viewport);

        void render() override;

        // Get current camera index for highlighting
        int getCurrentCameraIndex() const { return current_camera_idx_; }

        // Navigation methods
        void nextCamera();
        void previousCamera();
        void setShowImageOverlay(bool show) { show_image_overlay_ = show; }

        // Check if image overlay should be shown
        bool shouldShowImageOverlay() const { return show_image_overlay_; }

        // Get current camera's image for overlay
        torch::Tensor getCurrentImage();

    private:
        void renderCameraControls();
        void renderImageControls();
        void renderDatasetInfo();
        void loadCurrentCameraImage();
        void jumpToCamera(int index);

        std::shared_ptr<CameraDataset> dataset_;
        CameraFrustumRenderer* frustum_renderer_;
        Viewport* viewport_;

        // State
        int current_camera_idx_ = 0;
        bool show_train_cameras_ = true;
        bool show_test_cameras_ = true;
        bool show_image_overlay_ = false;
        float frustum_scale_ = 2.0f;  // Increased default scale

        // Cached data
        torch::Tensor current_image_;
        std::vector<int> train_indices_;
        std::vector<int> test_indices_;
        std::vector<bool> is_test_camera_;
    };

} // namespace gs