#pragma once

#include "core/dataset.hpp"
#include "visualizer/gui/gui_manager.hpp"
#include <memory>
#include <torch/torch.h>
#include <vector>

// Forward declarations
class Viewport; // Viewport is not in gs namespace

namespace gs {

    class CameraFrustumRenderer;

    class DatasetViewerPanel : public GUIPanel {
    public:
        DatasetViewerPanel(std::shared_ptr<CameraDataset> dataset,
                           CameraFrustumRenderer* frustum_renderer,
                           Viewport* viewport);

        void render() override;

        // Public methods for external access
        int getCurrentCameraIndex() const { return current_camera_idx_; }
        bool shouldShowImageOverlay() const { return show_image_overlay_; }
        torch::Tensor getCurrentImage() const { return current_image_; }

    private:
        void renderDatasetInfo();
        void renderCameraControls();
        void renderImageControls();

        void loadCurrentCameraImage();
        void jumpToCamera(int index);
        void previousCamera();
        void nextCamera();

        std::shared_ptr<CameraDataset> dataset_;
        CameraFrustumRenderer* frustum_renderer_;
        Viewport* viewport_;

        // Camera visualization state
        bool show_train_cameras_ = true;
        bool show_test_cameras_ = true;
        float frustum_scale_ = 1.0f;

        // Image viewing state
        int current_camera_idx_ = 0;
        bool show_image_overlay_ = false;
        torch::Tensor current_image_;

        // Camera indices
        std::vector<int> train_indices_;
        std::vector<int> test_indices_;
        std::vector<bool> is_test_camera_;
    };

} // namespace gs