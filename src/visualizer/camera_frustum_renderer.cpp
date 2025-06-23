#include "visualizer/camera_frustum_renderer.hpp"
#include "visualizer/gl_headers.hpp"
#include "visualizer/viewport.hpp"
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

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
        // Create a pyramid frustum matching the GitHub example
        // In the GitHub code: apex at -0.5, base at +0.5
        // This creates a frustum pointing along +Z in camera space
        std::vector<glm::vec3> vertices = {
            // Apex (camera position) - behind the base
            glm::vec3( 0.0f,  0.0f, -0.5f),  // 0
            // Base vertices (image plane, in front along +Z)
            glm::vec3(-0.5f, -0.5f,  0.5f),  // 1 bottom-left
            glm::vec3( 0.5f, -0.5f,  0.5f),  // 2 bottom-right
            glm::vec3( 0.5f,  0.5f,  0.5f),  // 3 top-right
            glm::vec3(-0.5f,  0.5f,  0.5f),  // 4 top-left
        };

        // Indices for line drawing (wireframe)
        std::vector<unsigned int> indices = {
            // Base rectangle
            1, 2,  2, 3,  3, 4,  4, 1,
            // Lines from apex to corners
            0, 1,  0, 2,  0, 3,  0, 4
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

        // Upload index data
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
                     indices.data(), GL_STATIC_DRAW);

        glBindVertexArray(0);
        std::cout << "Frustum geometry created with " << vertices.size()
                  << " vertices and " << indices.size() << " indices" << std::endl;
    }


    void CameraFrustumRenderer::setCameras(const std::vector<std::shared_ptr<Camera>>& cameras,
                                           const std::vector<bool>& is_test_camera) {
        cameras_ = cameras;
        is_test_camera_ = is_test_camera;

        std::cout << "Setting " << cameras.size() << " cameras" << std::endl;

        // Don't calculate scene bounds here - cameras might not be initialized yet
        // Just set a default scale
        frustum_scale_ = 0.1f;

        updateInstanceBuffer();
    }

    void CameraFrustumRenderer::calculateSceneBounds() {
        if (cameras_.empty())
            return;

        std::vector<glm::vec3> positions;
        positions.reserve(cameras_.size());

        for (size_t i = 0; i < cameras_.size(); ++i) {
            try {
                const auto& cam = cameras_[i];
                auto w2c_t = cam->world_view_transform();
                if (!w2c_t.defined() || w2c_t.numel() == 0)
                    continue;

                // IMPORTANT: Do NOT transpose - it's already in the correct form!
                w2c_t = w2c_t.to(torch::kCPU).squeeze(0);

                // Extract R and t
                torch::Tensor R = w2c_t.slice(0, 0, 3).slice(1, 0, 3);
                torch::Tensor t = w2c_t.slice(0, 0, 3).slice(1, 3);

                // Camera position: C = -R^T * t
                torch::Tensor cam_pos = -torch::matmul(R.transpose(0, 1), t.squeeze());
                auto pos_data = cam_pos.accessor<float, 1>();

                // Apply coordinate transformation for display
                glm::vec3 cam_pos_opengl(pos_data[0], -pos_data[1], -pos_data[2]);
                positions.push_back(cam_pos_opengl);

            } catch (const std::exception& e) {
                std::cerr << "Error calculating bounds for camera " << i << ": " << e.what() << std::endl;
            }
        }

        if (positions.empty()) {
            std::cerr << "No valid camera positions found!" << std::endl;
            return;
        }

        // Calculate centroid
        scene_center_ = glm::vec3(0.0f);
        for (const auto& pos : positions) {
            scene_center_ += pos;
        }
        scene_center_ /= positions.size();

        // Calculate radius
        scene_radius_ = 0.0f;
        for (const auto& pos : positions) {
            float dist = glm::length(pos - scene_center_);
            scene_radius_ = std::max(scene_radius_, dist);
        }

        std::cout << "Scene bounds - Center: (" << scene_center_.x << ", "
                  << scene_center_.y << ", " << scene_center_.z
                  << "), Radius: " << scene_radius_ << std::endl;
        std::cout << "Found " << positions.size() << " valid camera positions" << std::endl;
    }

    void CameraFrustumRenderer::updateInstanceBuffer() {
        if (!initialized_ || cameras_.empty()) {
            return;
        }

        std::vector<InstanceData> instance_data;
        instance_data.reserve(cameras_.size());

        int valid_cameras = 0;

        for (size_t i = 0; i < cameras_.size(); ++i) {
            // Skip if visibility doesn't match
            if (!((is_test_camera_[i] && show_test_) || (!is_test_camera_[i] && show_train_))) {
                continue;
            }

            try {
                const auto& cam = cameras_[i];
                InstanceData data;

                // Get world-to-camera transform
                // IMPORTANT: This matrix is already transposed in the Camera constructor!
                auto w2c_tensor = cam->world_view_transform();
                if (!w2c_tensor.defined() || w2c_tensor.size(0) != 1 || w2c_tensor.size(1) != 4 || w2c_tensor.size(2) != 4) {
                    std::cerr << "Camera " << i << " has invalid transform, skipping" << std::endl;
                    continue;
                }

                // Convert to CPU and squeeze batch dimension
                w2c_tensor = w2c_tensor.to(torch::kCPU).squeeze(0);

                // Extract the 3x3 rotation and 3x1 translation
                torch::Tensor R = w2c_tensor.slice(0, 0, 3).slice(1, 0, 3);
                torch::Tensor t = w2c_tensor.slice(0, 0, 3).slice(1, 3);

                // Convert to GLM for easier manipulation (matching GitHub example)
                auto R_data = R.accessor<float, 2>();
                torch::Tensor t_squeezed = t.squeeze();
                auto t_data = t_squeezed.accessor<float, 1>();

                // Build Rwc (camera-to-world rotation) = R^T
                glm::mat4 Rwc = glm::mat4(1.0f);
                for (int row = 0; row < 3; ++row) {
                    for (int col = 0; col < 3; ++col) {
                        Rwc[col][row] = R_data[row][col]; // Transpose from torch to glm
                    }
                }

                // Camera position in world space (matching GitHub)
                glm::vec3 tvec(t_data[0], t_data[1], t_data[2]);
                glm::vec3 worldPos = -glm::vec3(Rwc * glm::vec4(tvec, 1.0));
                glm::vec3 position = glm::vec3(worldPos.x, -worldPos.y, -worldPos.z);

                // Looking direction is the third column of Rwc (matching GitHub)
                glm::vec3 worldDir = -glm::vec3(Rwc[2]); // Camera looks along -Z in world space
                glm::vec3 lookingDir = glm::normalize(glm::vec3(worldDir.x, -worldDir.y, -worldDir.z));

                // Set orientation using quatLookAt (matching GitHub)
                glm::quat orientation = glm::quatLookAt(lookingDir, glm::vec3(0.0f, 1.0f, 0.0f));

                // Build the final transformation matrix
                glm::mat4 c2w = glm::mat4(1.0f);
                c2w = glm::translate(c2w, position);
                c2w = c2w * glm::mat4_cast(orientation);
                // Note: GitHub example also scales by 0.2, but we handle that in the shader

                data.camera_to_world = c2w;

                // Debug: print camera info
                if (i < 3 || (i % 10 == 0)) {
                    std::cout << "Camera " << i << " position: ("
                              << position.x << ", "
                              << position.y << ", "
                              << position.z << ")" << std::endl;

                    std::cout << "Camera " << i << " looking direction: ("
                              << lookingDir.x << ", "
                              << lookingDir.y << ", "
                              << lookingDir.z << ")" << std::endl;
                }

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
        std::cout << "Scene radius: " << scene_radius_ << ", Frustum scale: " << frustum_scale_ << std::endl;

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

        // Count visible cameras and debug first few positions
        int visible_count = 0;
        static bool debug_printed = false;
        if (!debug_printed) {
            std::cout << "\n=== Camera Frustum Positions in Render ===" << std::endl;
            for (size_t i = 0; i < std::min(size_t(5), cameras_.size()); ++i) {
                if ((is_test_camera_[i] && show_test_) || (!is_test_camera_[i] && show_train_)) {
                    const auto& cam = cameras_[i];
                    auto w2c_t = cam->world_view_transform();
                    if (w2c_t.defined() && w2c_t.numel() > 0) {
                        w2c_t = w2c_t.to(torch::kCPU).squeeze(0);
                        auto R = w2c_t.slice(0, 0, 3).slice(1, 0, 3);
                        auto t = w2c_t.slice(0, 0, 3).slice(1, 3);
                        auto cam_pos = -torch::matmul(R.transpose(0, 1), t.squeeze());
                        auto pos_data = cam_pos.accessor<float, 1>();
                        std::cout << "Camera " << i << " COLMAP pos: ("
                                  << pos_data[0] << ", " << pos_data[1] << ", " << pos_data[2] << ")" << std::endl;
                        std::cout << "Camera " << i << " OpenGL pos: ("
                                  << pos_data[0] << ", " << -pos_data[1] << ", " << -pos_data[2] << ")" << std::endl;
                    }
                }
            }
            debug_printed = true;
        }

        for (size_t i = 0; i < cameras_.size(); ++i) {
            if ((is_test_camera_[i] && show_test_) || (!is_test_camera_[i] && show_train_)) {
                visible_count++;
            }
        }

        if (visible_count == 0) {
            return;
        }

        // Enable depth test and disable face culling for lines
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Enable line smoothing for nicer appearance
        glEnable(GL_LINE_SMOOTH);
        glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
        glLineWidth(2.5f);

        // Bind shader and set uniforms
        frustum_shader_->bind();

        glm::mat4 view = viewport.getViewMatrix();
        glm::mat4 projection = viewport.getProjectionMatrix();
        glm::mat4 viewProj = projection * view;
        glm::vec3 viewPos = viewport.getCameraPosition();

        frustum_shader_->set_uniform("viewProj", viewProj);
        frustum_shader_->set_uniform("frustumScale", frustum_scale_);
        frustum_shader_->set_uniform("highlightIndex", highlight_index);
        frustum_shader_->set_uniform("highlightColor", highlight_color_);
        frustum_shader_->set_uniform("viewPos", viewPos);
        frustum_shader_->set_uniform("enableShading", true);

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

        // Restore line width and disable line smoothing
        glLineWidth(1.0f);
        glDisable(GL_LINE_SMOOTH);
    }

} // namespace gs