#include "visualizer/gui/dataset_viewer_panel.hpp"
#include "visualizer/camera_frustum_renderer.hpp"
#include "visualizer/viewport.hpp"
#include <imgui.h>
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace gs {

    DatasetViewerPanel::DatasetViewerPanel(
        std::shared_ptr<CameraDataset> dataset,
        CameraFrustumRenderer* frustum_renderer,
        Viewport* viewport)
        : GUIPanel("Dataset Viewer"),
          dataset_(dataset),
          frustum_renderer_(frustum_renderer),
          viewport_(viewport) {

        std::cout << "DatasetViewerPanel constructor called" << std::endl;

        if (!dataset_) {
            std::cerr << "Warning: DatasetViewerPanel created with null dataset!" << std::endl;
            return;
        }

        if (!frustum_renderer_) {
            std::cerr << "Warning: DatasetViewerPanel created with null frustum renderer!" << std::endl;
            return;
        }

        // Initialize camera indices
        const auto& cameras = dataset_->get_cameras();
        std::cout << "Dataset has " << cameras.size() << " cameras" << std::endl;

        is_test_camera_.resize(cameras.size());

        // Separate train and test indices based on dataset config
        for (size_t i = 0; i < cameras.size(); ++i) {
            // This matches the logic in CameraDataset constructor
            const bool is_test = (i % 8) == 0; // Default test_every=8
            is_test_camera_[i] = is_test;

            if (is_test) {
                test_indices_.push_back(i);
            } else {
                train_indices_.push_back(i);
            }
        }

        std::cout << "Train cameras: " << train_indices_.size()
                  << ", Test cameras: " << test_indices_.size() << std::endl;

        // Pass cameras to frustum renderer
        frustum_renderer_->setCameras(cameras, is_test_camera_);

        // Debug: print some camera positions
        std::cout << "Dataset viewer initialized with " << cameras.size() << " cameras" << std::endl;
        if (cameras.size() > 0) {
            // Print first few camera positions for debugging
            for (size_t i = 0; i < std::min(size_t(3), cameras.size()); ++i) {
                try {
                    const auto& cam = cameras[i];
                    // Check if camera has valid transforms
                    if (!cam->world_view_transform().defined() ||
                        cam->world_view_transform().numel() == 0) {
                        std::cerr << "Camera " << i << " has invalid world_view_transform!" << std::endl;
                        continue;
                    }

                    // Get camera position from world_view_transform
                    auto w2c_t = cam->world_view_transform();
                    if (w2c_t.defined() && w2c_t.numel() > 0) {
                        w2c_t = w2c_t.to(torch::kCPU).squeeze(0).transpose(0, 1);
                        auto R = w2c_t.slice(0, 0, 3).slice(1, 0, 3);
                        auto t = w2c_t.slice(0, 0, 3).slice(1, 3);
                        auto cam_pos = -torch::matmul(R.transpose(0, 1), t.squeeze());

                        auto pos_data = cam_pos.accessor<float, 1>();
                        std::cout << "Camera " << i << " position: ("
                                  << pos_data[0] << ", "
                                  << pos_data[1] << ", "
                                  << pos_data[2] << ")" << std::endl;
                    } else {
                        std::cerr << "Camera " << i << " has no valid transform!" << std::endl;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error accessing camera " << i << ": " << e.what() << std::endl;
                }
            }
        }

        // Calculate scene bounds for better initial view
        if (cameras.size() > 0) {
            torch::Tensor all_centers = torch::zeros({static_cast<long>(cameras.size()), 3});
            int valid_count = 0;

            for (size_t i = 0; i < cameras.size(); ++i) {
                try {
                    auto w2c_t = cameras[i]->world_view_transform();
                    if (w2c_t.defined() && w2c_t.numel() > 0) {
                        w2c_t = w2c_t.to(torch::kCPU).squeeze(0).transpose(0, 1);
                        auto R = w2c_t.slice(0, 0, 3).slice(1, 0, 3);
                        auto t = w2c_t.slice(0, 0, 3).slice(1, 3);
                        auto cam_pos = -torch::matmul(R.transpose(0, 1), t.squeeze());
                        all_centers[valid_count] = cam_pos;
                        valid_count++;
                    }
                } catch (...) {
                    // Skip invalid cameras
                }
            }

            if (valid_count > 0) {
                all_centers = all_centers.slice(0, 0, valid_count);
                auto mean_pos = all_centers.mean(0);
                auto distances = (all_centers - mean_pos).norm(2, 1);
                auto max_dist = distances.max().item<float>();

                auto mean_data = mean_pos.accessor<float, 1>();
                std::cout << "Scene bounds - Center: ("
                          << mean_data[0] << ", " << mean_data[1] << ", " << mean_data[2]
                          << "), Radius: " << max_dist << std::endl;
            }
        }
    }

    void DatasetViewerPanel::render() {
        if (!visible_) return;

        ImGui::Begin(title_.c_str(), &visible_);

        if (!dataset_) {
            ImGui::Text("No dataset loaded");
            ImGui::End();
            return;
        }

        renderDatasetInfo();
        ImGui::Separator();
        renderCameraControls();
        ImGui::Separator();
        renderImageControls();

        ImGui::End();
    }

    void DatasetViewerPanel::renderDatasetInfo() {
        ImGui::Text("Dataset Information");
        ImGui::Text("Total Cameras: %zu", dataset_->get_cameras().size());
        ImGui::Text("Train Cameras: %zu", train_indices_.size());
        ImGui::Text("Test Cameras: %zu", test_indices_.size());

        const auto& cam = dataset_->get_cameras()[0];
        ImGui::Text("Image Resolution: %dx%d", cam->image_width(), cam->image_height());
    }

    void DatasetViewerPanel::renderCameraControls() {
        ImGui::Text("Camera Visualization");

        // Visibility toggles
        if (ImGui::Checkbox("Show Train Cameras", &show_train_cameras_)) {
            frustum_renderer_->setShowTrainCameras(show_train_cameras_);
        }

        if (ImGui::Checkbox("Show Test Cameras", &show_test_cameras_)) {
            frustum_renderer_->setShowTestCameras(show_test_cameras_);
        }

        // Frustum scale
        if (ImGui::SliderFloat("Frustum Scale", &frustum_scale_, 0.1f, 10.0f)) {
            frustum_renderer_->setFrustumScale(frustum_scale_);
        }

        // Camera navigation
        ImGui::Separator();
        ImGui::Text("Navigate Cameras");

        // Camera index slider
        int camera_count = static_cast<int>(dataset_->get_cameras().size());
        if (ImGui::SliderInt("Camera Index", &current_camera_idx_, 0, camera_count - 1)) {
            loadCurrentCameraImage();
        }

        // Previous/Next buttons
        if (ImGui::Button("< Prev")) {
            previousCamera();
        }
        ImGui::SameLine();
        if (ImGui::Button("Next >")) {
            nextCamera();
        }
        ImGui::SameLine();
        if (ImGui::Button("Jump To View")) {
            jumpToCamera(current_camera_idx_);
        }

        // Show camera info
        const auto& current_cam = dataset_->get_cameras()[current_camera_idx_];
        ImGui::Text("Current: %s", current_cam->image_name().c_str());
        ImGui::Text("Type: %s", is_test_camera_[current_camera_idx_] ? "Test" : "Train");

        // Show camera position
        try {
            auto w2c_t = current_cam->world_view_transform();
            if (w2c_t.defined() && w2c_t.numel() > 0) {
                w2c_t = w2c_t.to(torch::kCPU).squeeze(0).transpose(0, 1);
                auto R = w2c_t.slice(0, 0, 3).slice(1, 0, 3);
                auto t = w2c_t.slice(0, 0, 3).slice(1, 3);
                auto cam_pos = -torch::matmul(R.transpose(0, 1), t.squeeze());
                auto pos_data = cam_pos.accessor<float, 1>();
                ImGui::Text("Position: (%.2f, %.2f, %.2f)",
                            pos_data[0], pos_data[1], pos_data[2]);
            } else {
                ImGui::Text("Position: (invalid)");
            }
        } catch (...) {
            ImGui::Text("Position: (invalid)");
        }
    }

    void DatasetViewerPanel::renderImageControls() {
        ImGui::Text("Image Display");

        ImGui::Checkbox("Show Image Overlay", &show_image_overlay_);

        if (show_image_overlay_ && current_image_.defined()) {
            ImGui::Text("Image: %ldx%ld", current_image_.size(1), current_image_.size(2));
        }
    }

    void DatasetViewerPanel::loadCurrentCameraImage() {
        if (current_camera_idx_ < 0 || current_camera_idx_ >= static_cast<int>(dataset_->get_cameras().size())) {
            return;
        }

        const auto& cam = dataset_->get_cameras()[current_camera_idx_];

        try {
            // For now, just load a placeholder or skip image loading
            // since original_image() doesn't exist
            current_image_ = torch::Tensor();
            std::cerr << "Image loading not implemented for camera " << current_camera_idx_ << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error loading image for camera " << current_camera_idx_ << ": " << e.what() << std::endl;
            current_image_ = torch::Tensor();
        }
    }

    void DatasetViewerPanel::jumpToCamera(int index) {
        if (!viewport_ || index < 0 || index >= static_cast<int>(dataset_->get_cameras().size())) {
            return;
        }

        const auto& cam = dataset_->get_cameras()[index];

        try {
            // Get camera position from world_view_transform
            torch::Tensor w2c_t = cam->world_view_transform();
            if (!w2c_t.defined() || w2c_t.numel() == 0) {
                std::cerr << "Invalid camera transform for jump" << std::endl;
                return;
            }

            w2c_t = w2c_t.to(torch::kCPU).squeeze(0).transpose(0, 1);
            torch::Tensor R = w2c_t.slice(0, 0, 3).slice(1, 0, 3);
            torch::Tensor t = w2c_t.slice(0, 0, 3).slice(1, 3);
            torch::Tensor cam_center = -torch::matmul(R.transpose(0, 1), t.squeeze());

            if (cam_center.numel() != 3) {
                std::cerr << "Invalid camera position for jump" << std::endl;
                return;
            }

            auto center_data = cam_center.accessor<float, 1>();
            glm::vec3 position(center_data[0], center_data[1], center_data[2]);

            // Extract view direction from world_view_transform
            // The view direction is the negative Z axis of the camera
            auto R_data = R.accessor<float, 2>();
            glm::vec3 forward(-R_data[2][0], -R_data[2][1], -R_data[2][2]);
            glm::vec3 up(R_data[1][0], R_data[1][1], R_data[1][2]);

            // Normalize directions
            forward = glm::normalize(forward);
            up = glm::normalize(up);

            // Set camera to look at the scene from this position
            viewport_->target = position + forward * 5.0f; // Look 5 units ahead
            viewport_->azimuth = std::atan2(forward.x, forward.z) * 180.0f / M_PI;
            viewport_->elevation = std::asin(-forward.y) * 180.0f / M_PI;
            viewport_->distance = 0.1f; // Very close to camera position

            std::cout << "Jumped to camera " << index << " at position ("
                      << position.x << ", " << position.y << ", " << position.z << ")" << std::endl;

        } catch (const std::exception& e) {
            std::cerr << "Error jumping to camera: " << e.what() << std::endl;
        }
    }

    void DatasetViewerPanel::nextCamera() {
        int camera_count = static_cast<int>(dataset_->get_cameras().size());
        current_camera_idx_ = std::min(camera_count - 1, current_camera_idx_ + 1);
        loadCurrentCameraImage();
    }

    void DatasetViewerPanel::previousCamera() {
        current_camera_idx_ = std::max(0, current_camera_idx_ - 1);
        loadCurrentCameraImage();
    }

    torch::Tensor DatasetViewerPanel::getCurrentImage() {
        if (current_camera_idx_ < 0 || current_camera_idx_ >= static_cast<int>(dataset_->get_cameras().size())) {
            return torch::Tensor();
        }

        if (!current_image_.defined()) {
            loadCurrentCameraImage();
        }

        return current_image_;
    }

} // namespace gs