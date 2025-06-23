#pragma once

#include "visualizer/shader.hpp"
#include "visualizer/viewport.hpp"
#include "core/camera.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace gs {

    class CameraFrustumRenderer {
    public:
        CameraFrustumRenderer();
        ~CameraFrustumRenderer();

        // Initialize the renderer with shader paths
        bool init(const std::string& shader_base_path);

        // Set cameras to visualize
        void setCameras(const std::vector<std::shared_ptr<Camera>>& cameras,
                        const std::vector<bool>& is_test_camera);

        // Render all camera frustums
        void render(const Viewport& viewport, int highlight_index = -1);

        // Set visualization options
        void setShowTrainCameras(bool show) { show_train_ = show; }
        void setShowTestCameras(bool show) { show_test_ = show; }
        void setFrustumScale(float scale) { frustum_scale_ = scale; }

    private:
        void createFrustumGeometry();
        void updateInstanceBuffer();

        std::shared_ptr<Shader> frustum_shader_;

        GLuint vao_, vbo_, ebo_;
        GLuint instance_vbo_;

        std::vector<std::shared_ptr<Camera>> cameras_;
        std::vector<bool> is_test_camera_;

        bool show_train_ = true;
        bool show_test_ = true;
        float frustum_scale_ = 2.0f;  // Increased default scale

        int num_indices_ = 0;
        bool initialized_ = false;

        // Colors
        const glm::vec3 train_color_ = glm::vec3(0.2f, 0.8f, 0.2f);
        const glm::vec3 test_color_ = glm::vec3(0.8f, 0.2f, 0.2f);
        const glm::vec3 highlight_color_ = glm::vec3(1.0f, 1.0f, 0.0f);
    };

} // namespace gs