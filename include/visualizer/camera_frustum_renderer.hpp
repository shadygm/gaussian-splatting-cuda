#pragma once

#include "core/camera.hpp"
#include "visualizer/shader.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <vector>

// Forward declaration
class Viewport; // Viewport is not in gs namespace

namespace gs {

    class CameraFrustumRenderer {
    public:
        CameraFrustumRenderer();
        ~CameraFrustumRenderer();

        bool init(const std::string& shader_base_path);
        void setCameras(const std::vector<std::shared_ptr<Camera>>& cameras,
                        const std::vector<bool>& is_test_camera);
        void render(const Viewport& viewport, int highlight_index = -1);

        void setShowTrainCameras(bool show) { show_train_ = show; }
        void setShowTestCameras(bool show) { show_test_ = show; }
        void setFrustumScale(float scale) { frustum_scale_ = scale; }
        float getFrustumScale() const { return frustum_scale_; }
        void setSceneTransform(const glm::mat4& transform) { scene_transform_ = transform; }

    private:
        void createFrustumGeometry();
        void updateInstanceBuffer();

        struct InstanceData {
            glm::mat4 camera_to_world;
            glm::vec3 color;
            float fov_x;
            float fov_y;
            float aspect;
            float padding[2]; // Align to 16 bytes
        };

        std::shared_ptr<Shader> frustum_shader_;
        std::vector<std::shared_ptr<Camera>> cameras_;
        std::vector<bool> is_test_camera_;

        unsigned int vao_ = 0;
        unsigned int vbo_ = 0;
        unsigned int face_ebo_ = 0;
        unsigned int edge_ebo_ = 0;
        unsigned int instance_vbo_ = 0;
        unsigned int num_face_indices_ = 0;
        unsigned int num_edge_indices_ = 0;

        bool show_train_ = true;
        bool show_test_ = true;
        float frustum_scale_ = .01f;

        glm::vec3 train_color_ = glm::vec3(0.3f, 0.8f, 0.3f);     // Nice green for train
        glm::vec3 test_color_ = glm::vec3(0.9f, 0.3f, 0.3f);      // Nice red for test
        glm::vec3 highlight_color_ = glm::vec3(1.0f, 0.9f, 0.2f); // Bright yellow for highlight

        glm::vec3 scene_center_ = glm::vec3(0.0f);
        float scene_radius_ = 1.0f;
        glm::mat4 scene_transform_{1.0f};

        bool initialized_ = false;
        bool auto_scale_ = true;
    };

} // namespace gs