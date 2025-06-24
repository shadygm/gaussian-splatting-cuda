#include "visualizer/camera_frustum_renderer.hpp"
#include "visualizer/gl_headers.hpp"
#include "visualizer/opengl_state_manager.hpp"
#include "visualizer/viewport.hpp"
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <iostream>

namespace gs {

    CameraFrustumRenderer::CameraFrustumRenderer() {}

    CameraFrustumRenderer::~CameraFrustumRenderer() {
        if (initialized_) {
            glDeleteVertexArrays(1, &vao_);
            glDeleteBuffers(1, &vbo_);
            glDeleteBuffers(1, &face_ebo_);
            glDeleteBuffers(1, &edge_ebo_);
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
        // Create a pyramid frustum matching the trackball example
        // In camera space: -Z is forward (viewing direction), +Y is up, +X is right
        std::vector<glm::vec3> vertices = {
            // Base vertices (in XY plane, at z = 0.5)
            glm::vec3(-0.5f, -0.5f, 0.5f), // 0 bottom-left
            glm::vec3(0.5f, -0.5f, 0.5f),  // 1 bottom-right
            glm::vec3(0.5f, 0.5f, 0.5f),   // 2 top-right
            glm::vec3(-0.5f, 0.5f, 0.5f),  // 3 top-left
            // Apex (camera position, at z = -0.5)
            glm::vec3(0.0f, 0.0f, -0.5f), // 4
        };

        // Indices for solid faces (triangles)
        std::vector<unsigned int> face_indices = {
            // Base (facing +Z)
            0, 1, 2,
            0, 2, 3,
            // Sides
            0, 4, 1, // Bottom face
            1, 4, 2, // Right face
            2, 4, 3, // Top face
            3, 4, 0  // Left face
        };

        // Indices for wireframe edges (lines)
        std::vector<unsigned int> edge_indices = {
            0, 1, // Base edges
            1, 2,
            2, 3,
            3, 0,
            0, 4, // Edges to apex
            1, 4,
            2, 4,
            3, 4};

        num_face_indices_ = face_indices.size();
        num_edge_indices_ = edge_indices.size();

        // Create VAO, VBO, and separate EBOs for faces and edges
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        glGenBuffers(1, &face_ebo_);
        glGenBuffers(1, &edge_ebo_);

        glBindVertexArray(vao_);

        // Upload vertex data
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3),
                     vertices.data(), GL_STATIC_DRAW);

        // Set vertex attributes
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

        // Upload face indices
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, face_ebo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, face_indices.size() * sizeof(unsigned int),
                     face_indices.data(), GL_STATIC_DRAW);

        // Upload edge indices
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, edge_ebo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, edge_indices.size() * sizeof(unsigned int),
                     edge_indices.data(), GL_STATIC_DRAW);

        glBindVertexArray(0);
        std::cout << "Frustum geometry created with " << vertices.size()
                  << " vertices, " << face_indices.size() << " face indices, and "
                  << edge_indices.size() << " edge indices" << std::endl;
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
                auto w2c_tensor = cam->world_view_transform();
                if (!w2c_tensor.defined() || w2c_tensor.size(0) != 1 || w2c_tensor.size(1) != 4 || w2c_tensor.size(2) != 4) {
                    std::cerr << "Camera " << i << " has invalid transform, skipping" << std::endl;
                    continue;
                }

                // Convert to CPU and squeeze batch dimension
                w2c_tensor = w2c_tensor.to(torch::kCPU).squeeze(0);

                // Extract R and t from the world-to-camera matrix
                // The matrix is [R t; 0 1] where R is the rotation matrix
                torch::Tensor R = w2c_tensor.slice(0, 0, 3).slice(1, 0, 3);
                torch::Tensor t = w2c_tensor.slice(0, 0, 3).slice(1, 3);

                // Convert to GLM
                auto R_data = R.accessor<float, 2>();
                torch::Tensor t_squeezed = t.squeeze();
                auto t_data = t_squeezed.accessor<float, 1>();

                // Build rotation matrix (world to camera)
                glm::mat3 Rwc_3x3;
                for (int row = 0; row < 3; ++row) {
                    for (int col = 0; col < 3; ++col) {
                        Rwc_3x3[col][row] = R_data[row][col]; // GLM is column-major
                    }
                }

                // Get camera to world rotation (transpose)
                glm::mat4 Rwc = glm::transpose(glm::mat4(Rwc_3x3));

                // Camera position in world space
                glm::vec3 tvec(t_data[0], t_data[1], t_data[2]);
                glm::vec3 worldPos = -glm::vec3(Rwc * glm::vec4(tvec, 1.0));

                // Convert from COLMAP to OpenGL coordinate system
                glm::vec3 position = glm::vec3(worldPos.x, -worldPos.y, -worldPos.z);

                // Looking direction is the third row of Rwc
                glm::vec3 worldDir = -glm::vec3(Rwc[2]); // Camera looks along -Z in world space
                glm::vec3 lookingDir = glm::normalize(glm::vec3(worldDir.x, -worldDir.y, -worldDir.z));

                // Set orientation using quatLookAt
                glm::quat orientation = glm::quatLookAt(lookingDir, glm::vec3(0.0f, 1.0f, 0.0f));

                // Build the final transformation matrix
                glm::mat4 c2w = glm::mat4(1.0f);
                c2w = glm::translate(c2w, position);
                c2w = c2w * glm::mat4_cast(orientation);
                c2w = glm::scale(c2w, glm::vec3(frustum_scale_));

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
            return;
        }

        // Use state guard for automatic restoration
        OpenGLStateManager::StateGuard guard(getGLStateManager());

        // First pass: Draw solid faces
        getGLStateManager().setForSolidFaces();

        // Bind shader
        frustum_shader_->bind();

        glm::mat4 view = viewport.getViewMatrix();
        glm::mat4 projection = viewport.getProjectionMatrix();
        glm::mat4 viewProj = projection * view;
        glm::vec3 viewPos = viewport.getCameraPosition();

        frustum_shader_->set_uniform("viewProj", viewProj);
        frustum_shader_->set_uniform("frustumScale", 1.0f); // Scale is already in the instance matrix
        frustum_shader_->set_uniform("highlightIndex", highlight_index);
        frustum_shader_->set_uniform("highlightColor", highlight_color_);
        frustum_shader_->set_uniform("viewPos", viewPos);
        frustum_shader_->set_uniform("enableShading", true);

        // Bind VAO
        glBindVertexArray(vao_);

        // Draw solid faces
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, face_ebo_);
        glDrawElementsInstanced(GL_TRIANGLES, num_face_indices_, GL_UNSIGNED_INT, 0, visible_count);

        // Second pass: Draw wireframe edges on top
        getGLStateManager().setForWireframe();

        // Make wireframe darker for better contrast
        frustum_shader_->set_uniform("enableShading", false);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, edge_ebo_);
        glDrawElementsInstanced(GL_LINES, num_edge_indices_, GL_UNSIGNED_INT, 0, visible_count);

        glBindVertexArray(0);
        frustum_shader_->unbind();
    }
} // namespace gs
