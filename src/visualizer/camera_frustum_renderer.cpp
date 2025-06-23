#include "visualizer/camera_frustum_renderer.hpp"
#include "visualizer/gl_headers.hpp"
#include <iostream>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace gs {

    CameraFrustumRenderer::CameraFrustumRenderer() {}

    CameraFrustumRenderer::~CameraFrustumRenderer() {
        if (initialized_) {
            glDeleteVertexArrays(1, &vao_);
            glDeleteBuffers(1, &vbo_);
            glDeleteBuffers(1, &ebo_);
            glDeleteBuffers(1, &instance_vbo_);
        }
    }

    bool CameraFrustumRenderer::init(const std::string& shader_base_path) {
        std::cout << "Initializing camera frustum renderer..." << std::endl;

        try {
            std::string vert_path = shader_base_path + "/camera_frustum.vert";
            std::string frag_path = shader_base_path + "/camera_frustum.frag";

            std::cout << "Loading camera frustum shaders from:" << std::endl;
            std::cout << "  Vertex: " << vert_path << std::endl;
            std::cout << "  Fragment: " << frag_path << std::endl;

            // Check if files exist
            if (!std::filesystem::exists(vert_path)) {
                std::cerr << "Camera frustum vertex shader not found: " << vert_path << std::endl;
                return false;
            }
            if (!std::filesystem::exists(frag_path)) {
                std::cerr << "Camera frustum fragment shader not found: " << frag_path << std::endl;
                return false;
            }

            frustum_shader_ = std::make_shared<Shader>(
                vert_path.c_str(),
                frag_path.c_str(),
                false);

            std::cout << "Camera frustum shaders loaded successfully" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Failed to load camera frustum shaders: " << e.what() << std::endl;
            return false;
        }

        std::cout << "Creating frustum geometry..." << std::endl;
        createFrustumGeometry();

        // Create instance buffer for per-camera data
        glGenBuffers(1, &instance_vbo_);
        std::cout << "Created instance buffer: " << instance_vbo_ << std::endl;

        initialized_ = true;
        std::cout << "Camera frustum renderer initialized successfully" << std::endl;
        return true;
    }

    void CameraFrustumRenderer::createFrustumGeometry() {
        // Create a simple frustum with 5 vertices (apex + 4 corners)
        std::vector<glm::vec3> vertices = {
            glm::vec3(0.0f, 0.0f, 0.0f),    // 0: Camera position (apex)
            glm::vec3(-1.0f, -1.0f, 1.0f),  // 1: Bottom-left far
            glm::vec3( 1.0f, -1.0f, 1.0f),  // 2: Bottom-right far
            glm::vec3( 1.0f,  1.0f, 1.0f),  // 3: Top-right far
            glm::vec3(-1.0f,  1.0f, 1.0f),  // 4: Top-left far
        };

        // Indices for line drawing
        std::vector<unsigned int> indices = {
            // Lines from apex to corners
            0, 1,  0, 2,  0, 3,  0, 4,
            // Rectangle at far plane
            1, 2,  2, 3,  3, 4,  4, 1
        };

        num_indices_ = indices.size();

        // Create VAO, VBO, EBO
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        glGenBuffers(1, &ebo_);

        glBindVertexArray(vao_);

        // Upload vertex data
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3),
                     vertices.data(), GL_STATIC_DRAW);

        // Set vertex attributes
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

        // Upload indices
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
                     indices.data(), GL_STATIC_DRAW);

        glBindVertexArray(0);
    }

    void CameraFrustumRenderer::setCameras(const std::vector<std::shared_ptr<Camera>>& cameras,
                                           const std::vector<bool>& is_test_camera) {
        cameras_ = cameras;
        is_test_camera_ = is_test_camera;

        std::cout << "CameraFrustumRenderer: Set " << cameras_.size() << " cameras" << std::endl;

        // Update instance buffer with new camera data
        if (initialized_) {
            updateInstanceBuffer();
        }
    }

    void CameraFrustumRenderer::updateInstanceBuffer() {
        if (cameras_.empty()) {
            std::cout << "No cameras to update in instance buffer" << std::endl;
            return;
        }

        struct InstanceData {
            glm::mat4 camera_to_world;
            glm::vec3 color;
            float fov_x;
            float fov_y;
            float aspect;
            float padding[2]; // Ensure proper alignment
        };

        std::vector<InstanceData> instance_data;
        instance_data.reserve(cameras_.size());

        int valid_cameras = 0;
        for (size_t i = 0; i < cameras_.size(); ++i) {
            const auto& cam = cameras_[i];

            // Skip cameras based on visibility settings
            if (is_test_camera_[i] && !show_test_) continue;
            if (!is_test_camera_[i] && !show_train_) continue;

            InstanceData data;

            try {
                // Get world_view_transform which is already transposed in Camera constructor
                // This is actually world-to-camera transform

                auto w2c_transposed = cam->world_view_transform();
                if (!w2c_transposed.defined() || w2c_transposed.size(0) != 1 || w2c_transposed.size(1) != 4 || w2c_transposed.size(2) != 4) {
                    std::cerr << "Camera " << i << " has invalid transform, skipping" << std::endl;
                    continue;
                }

                // Convert to CPU and get the actual transform
                w2c_transposed = w2c_transposed.to(torch::kCPU).squeeze(0);

                // We need the actual world-to-camera transform (transpose back)
                torch::Tensor w2c = w2c_transposed.transpose(0, 1);

                // Extract rotation and translation
                torch::Tensor R = w2c.slice(0, 0, 3).slice(1, 0, 3);
                torch::Tensor t = w2c.slice(0, 0, 3).slice(1, 3);

                // Camera position in world space: C = -R^T * t
                torch::Tensor cam_pos = -torch::matmul(R.transpose(0, 1), t.squeeze());

                // Build camera-to-world transform
                glm::mat4 c2w = glm::mat4(1.0f);

                // Set rotation (R^T)
                auto R_data = R.accessor<float, 2>();
                for (int i = 0; i < 3; ++i) {
                    for (int j = 0; j < 3; ++j) {
                        c2w[j][i] = R_data[i][j]; // Transpose
                    }
                }

                // Set translation
                auto pos_data = cam_pos.accessor<float, 1>();
                c2w[3][0] = pos_data[0];
                c2w[3][1] = pos_data[1];
                c2w[3][2] = pos_data[2];

                data.camera_to_world = c2w;

                // Set color based on train/test
                data.color = is_test_camera_[i] ? test_color_ : train_color_;

                // Camera parameters
                data.fov_x = cam->FoVx();
                data.fov_y = cam->FoVy();
                data.aspect = float(cam->image_width()) / float(cam->image_height());

                instance_data.push_back(data);
                valid_cameras++;

            } catch (const std::exception& e) {
                std::cerr << "Error processing camera " << i << ": " << e.what() << std::endl;
                continue;
            }
        }

        if (instance_data.empty()) {
            std::cerr << "No valid camera data to render!" << std::endl;
            return;
        }

        std::cout << "Updating instance buffer with " << valid_cameras << " valid cameras" << std::endl;

        // Upload instance data
        glBindBuffer(GL_ARRAY_BUFFER, instance_vbo_);
        glBufferData(GL_ARRAY_BUFFER, instance_data.size() * sizeof(InstanceData),
                     instance_data.data(), GL_DYNAMIC_DRAW);

        // Set up instance attributes
        glBindVertexArray(vao_);

        // camera_to_world matrix (4 vec4s)
        for (int i = 0; i < 4; ++i) {
            glEnableVertexAttribArray(1 + i);
            glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData),
                                  (void*)(sizeof(glm::vec4) * i));
            glVertexAttribDivisor(1 + i, 1);
        }

        // color
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, sizeof(InstanceData),
                              (void*)(offsetof(InstanceData, color)));
        glVertexAttribDivisor(5, 1);

        // fov and aspect
        glEnableVertexAttribArray(6);
        glVertexAttribPointer(6, 3, GL_FLOAT, GL_FALSE, sizeof(InstanceData),
                              (void*)(offsetof(InstanceData, fov_x)));
        glVertexAttribDivisor(6, 1);

        glBindVertexArray(0);
    }

    void CameraFrustumRenderer::render(const Viewport& viewport, int highlight_index) {
        if (!initialized_ || cameras_.empty()) {
            return;
        }

        // Update instance buffer with current visibility settings
        updateInstanceBuffer();

        // Count visible cameras
        int visible_count = 0;
        for (size_t i = 0; i < cameras_.size(); ++i) {
            if ((is_test_camera_[i] && show_test_) || (!is_test_camera_[i] && show_train_)) {
                visible_count++;
            }
        }

        if (visible_count == 0) {
            std::cout << "No visible cameras to render" << std::endl;
            return;
        }

        // Enable depth test and disable face culling for lines
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glLineWidth(2.0f);

        // Bind shader and set uniforms
        frustum_shader_->bind();

        glm::mat4 view = viewport.getViewMatrix();
        glm::mat4 projection = viewport.getProjectionMatrix();
        glm::mat4 viewProj = projection * view;

        frustum_shader_->set_uniform("viewProj", viewProj);
        frustum_shader_->set_uniform("frustumScale", frustum_scale_);
        frustum_shader_->set_uniform("highlightIndex", highlight_index);
        frustum_shader_->set_uniform("highlightColor", highlight_color_);

        // Bind VAO and draw instances
        glBindVertexArray(vao_);

        // Draw each visible camera as an instance
        int instance_id = 0;
        for (size_t i = 0; i < cameras_.size(); ++i) {
            if ((is_test_camera_[i] && show_test_) || (!is_test_camera_[i] && show_train_)) {
                frustum_shader_->set_uniform("currentIndex", static_cast<int>(i));
                glDrawElementsInstancedBaseInstance(GL_LINES, num_indices_, GL_UNSIGNED_INT, 0, 1, instance_id);
                instance_id++;
            }
        }

        glBindVertexArray(0);
        frustum_shader_->unbind();

        // Restore line width
        glLineWidth(1.0f);
    }

} // namespace gs