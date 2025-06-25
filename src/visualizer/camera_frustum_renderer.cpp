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
        // ------------------------------------------------------------
        // Pyramid defined in *OpenGL camera space*
        //   – Apex  at  (0,0,0)          (camera centre)
        //   – Base  at  z = -0.5         (looking down -Z)
        //
        // This way, after the single axis‑flip (y,z) the pyramid points
        // along COLMAP +Z and ends up facing into the scene.
        // ------------------------------------------------------------
        std::vector<glm::vec3> vertices = {
            // Base vertices  (CCW when seen from outside the frustum)
            {-0.5f, -0.5f, -0.5f}, // 0  bottom‑left
            {0.5f, -0.5f, -0.5f},  // 1  bottom‑right
            {0.5f, 0.5f, -0.5f},   // 2  top‑right
            {-0.5f, 0.5f, -0.5f},  // 3  top‑left
            // Apex (camera position)
            {0.0f, 0.0f, 0.0f} // 4
        };

        // Faces (18 indices, CCW from outside)
        std::vector<unsigned int> face_indices = {
            // Base (normal points towards +Z, i.e. inside – it's never visible)
            0, 1, 2,
            0, 2, 3,
            // Sides
            0, 4, 1,
            1, 4, 2,
            2, 4, 3,
            3, 4, 0};

        // Wireframe edges (unchanged)
        std::vector<unsigned int> edge_indices = {
            0, 1, 1, 2, 2, 3, 3, 0, // base square
            0, 4, 1, 4, 2, 4, 3, 4  // edges to apex
        };

        num_face_indices_ = face_indices.size();
        num_edge_indices_ = edge_indices.size();

        // ---------- OpenGL upload (same as before) ------------------
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        glGenBuffers(1, &face_ebo_);
        glGenBuffers(1, &edge_ebo_);

        glBindVertexArray(vao_);

        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER,
                     vertices.size() * sizeof(glm::vec3),
                     vertices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                              sizeof(glm::vec3), (void*)0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, face_ebo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     face_indices.size() * sizeof(unsigned int),
                     face_indices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, edge_ebo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     edge_indices.size() * sizeof(unsigned int),
                     edge_indices.data(), GL_STATIC_DRAW);

        glBindVertexArray(0);

        std::cout << "Frustum geometry rebuilt (apex at origin, base on -Z)\n";
    }

    void CameraFrustumRenderer::setCameras(const std::vector<std::shared_ptr<Camera>>& cameras,
                                           const std::vector<bool>& is_test_camera) {
        cameras_ = cameras;
        is_test_camera_ = is_test_camera;

        std::cout << "Setting " << cameras.size() << " cameras" << std::endl;

        // Calculate scene bounds from camera positions
        if (!cameras.empty()) {
            glm::vec3 min_pos(std::numeric_limits<float>::max());
            glm::vec3 max_pos(std::numeric_limits<float>::lowest());
            int valid_cameras = 0;

            for (const auto& cam : cameras) {
                try {
                    auto w2c_tensor = cam->world_view_transform();
                    if (!w2c_tensor.defined() || w2c_tensor.numel() == 0) {
                        continue;
                    }

                    w2c_tensor = w2c_tensor.to(torch::kCPU).squeeze(0);
                    torch::Tensor R = w2c_tensor.slice(0, 0, 3).slice(1, 0, 3);
                    torch::Tensor t = w2c_tensor.slice(0, 0, 3).slice(1, 3).squeeze();

                    // Camera position in world space
                    auto R_t = R.transpose(0, 1);
                    torch::Tensor cam_pos = -torch::matmul(R_t, t);

                    auto pos_data = cam_pos.accessor<float, 1>();
                    glm::vec3 pos(pos_data[0], -pos_data[1], -pos_data[2]);

                    min_pos = glm::min(min_pos, pos);
                    max_pos = glm::max(max_pos, pos);
                    valid_cameras++;
                } catch (...) {
                    // Skip invalid cameras
                }
            }

            if (valid_cameras > 0) {
                scene_center_ = (min_pos + max_pos) * 0.5f;
                scene_radius_ = glm::length(max_pos - min_pos) * 0.5f;

                // Calculate appropriate frustum scale based on scene size
                frustum_scale_ = scene_radius_ * 0.05f; // 5% of scene radius
                frustum_scale_ = glm::clamp(frustum_scale_, 0.01f, 1.0f);

                std::cout << "Scene bounds calculated - Center: ("
                          << scene_center_.x << ", " << scene_center_.y << ", " << scene_center_.z
                          << "), Radius: " << scene_radius_
                          << ", Frustum scale: " << frustum_scale_ << std::endl;
            }
        }

        updateInstanceBuffer();
    }

    void CameraFrustumRenderer::updateInstanceBuffer() {
        if (!initialized_ || cameras_.empty())
            return;

        const glm::mat4 GL_TO_COLMAP =
            glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, -1.0f));

        std::vector<InstanceData> instances;
        instances.reserve(cameras_.size());

        for (size_t i = 0; i < cameras_.size(); ++i) {
            if (!((is_test_camera_[i] && show_test_) ||
                  (!is_test_camera_[i] && show_train_)))
                continue;

            try {
                //------------------------------------------------------------------
                // 1) world→camera 4×4 from torch  →  CPU → glm
                //------------------------------------------------------------------
                torch::Tensor w2c = cameras_[i]->world_view_transform().to(torch::kCPU).squeeze(0);

                glm::mat4 w2c_glm(1.0f);
                auto m = w2c.accessor<float, 2>();
                for (int r = 0; r < 4; ++r)
                    for (int c = 0; c < 4; ++c)
                        w2c_glm[c][r] = m[r][c]; // row→column

                //------------------------------------------------------------------
                // 2) camera→world in COLMAP coords
                //------------------------------------------------------------------
                glm::mat4 c2w_colmap = glm::inverse(w2c_glm);

                //------------------------------------------------------------------
                // 3) build model matrix
                //        inverse(scene) · c2w · flip · scale
                //------------------------------------------------------------------
                glm::mat4 model =
                    glm::inverse(scene_transform_) * //  ←  FIX ▶
                    c2w_colmap * GL_TO_COLMAP *
                    glm::scale(glm::mat4(1.0f),
                               glm::vec3(frustum_scale_));

                //------------------------------------------------------------------
                // 4) pack instance
                //------------------------------------------------------------------
                InstanceData inst;
                inst.camera_to_world = model;
                inst.color = is_test_camera_[i] ? test_color_ : train_color_;
                inst.fov_x = cameras_[i]->FoVx();
                inst.fov_y = cameras_[i]->FoVy();
                inst.aspect = static_cast<float>(cameras_[i]->image_width()) /
                              static_cast<float>(cameras_[i]->image_height());

                instances.push_back(inst);
            } catch (const std::exception& e) {
                std::cerr << "Camera " << i << " error: " << e.what() << '\n';
            }
        }

        if (instances.empty()) {
            std::cerr << "No visible cameras to render.\n";
            return;
        }

        //---------------------------------------------------------------------------
        // 5) upload buffer & configure attributes (unchanged)
        //---------------------------------------------------------------------------
        glBindBuffer(GL_ARRAY_BUFFER, instance_vbo_);
        glBufferData(GL_ARRAY_BUFFER,
                     instances.size() * sizeof(InstanceData),
                     instances.data(),
                     GL_DYNAMIC_DRAW);

        glBindVertexArray(vao_);

        for (int col = 0; col < 4; ++col) {
            glEnableVertexAttribArray(1 + col);
            glVertexAttribPointer(1 + col, 4, GL_FLOAT, GL_FALSE,
                                  sizeof(InstanceData),
                                  reinterpret_cast<void*>(sizeof(glm::vec4) * col));
            glVertexAttribDivisor(1 + col, 1);
        }

        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE,
                              sizeof(InstanceData),
                              reinterpret_cast<void*>(offsetof(InstanceData, color)));
        glVertexAttribDivisor(5, 1);

        glEnableVertexAttribArray(6);
        glVertexAttribPointer(6, 3, GL_FLOAT, GL_FALSE,
                              sizeof(InstanceData),
                              reinterpret_cast<void*>(offsetof(InstanceData, fov_x)));
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

        // Bind VAO
        glBindVertexArray(vao_);

        // First pass: Draw solid faces
        getGLStateManager().setForSolidFaces();
        frustum_shader_->set_uniform("enableShading", true);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, face_ebo_);
        glDrawElementsInstanced(GL_TRIANGLES, num_face_indices_, GL_UNSIGNED_INT, 0, visible_count);

        // Second pass: Draw wireframe edges on top
        getGLStateManager().setForWireframe();
        frustum_shader_->set_uniform("enableShading", false);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, edge_ebo_);
        glDrawElementsInstanced(GL_LINES, num_edge_indices_, GL_UNSIGNED_INT, 0, visible_count);

        glBindVertexArray(0);
        frustum_shader_->unbind();
    }
} // namespace gs