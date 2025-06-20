#include "visualizer/infinite_grid_renderer.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <random>

namespace gs {

    InfiniteGridRenderer::InfiniteGridRenderer() {
        // Will be initialized in init()
    }

    InfiniteGridRenderer::~InfiniteGridRenderer() {
        if (initialized_) {
            glDeleteVertexArrays(1, &vao_);
            glDeleteBuffers(1, &vbo_);
            glDeleteTextures(1, &blue_noise_texture_);
        }
    }

    bool InfiniteGridRenderer::init(const std::string& shader_base_path) {
        // Load shaders
        try {
            grid_shader_ = std::make_shared<Shader>(
                (shader_base_path + "/infinite_grid.vert").c_str(),
                (shader_base_path + "/infinite_grid.frag").c_str(),
                false  // Don't create default buffers
            );
        } catch (const std::exception& e) {
            std::cerr << "Failed to load infinite grid shaders: " << e.what() << std::endl;
            return false;
        }

        // Create VAO and VBO for full-screen quad
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);

        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);

        // Full-screen quad vertices (-1 to 1)
        float vertices[] = {
            -1.0f, -1.0f,
            1.0f, -1.0f,
            -1.0f,  1.0f,
            1.0f,  1.0f
        };

        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        // Set up vertex attributes
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

        glBindVertexArray(0);

        // Create blue noise texture
        createBlueNoiseTexture();

        initialized_ = true;
        return true;
    }

    void InfiniteGridRenderer::createBlueNoiseTexture() {
        const int size = 32;
        std::vector<float> noise_data(size * size);

        // Generate blue noise pattern (simplified version)
        std::mt19937 rng(42);  // Fixed seed for consistency
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        for (int i = 0; i < size * size; ++i) {
            noise_data[i] = dist(rng);
        }

        // Create texture
        glGenTextures(1, &blue_noise_texture_);
        glBindTexture(GL_TEXTURE_2D, blue_noise_texture_);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, size, size, 0, GL_RED, GL_FLOAT, noise_data.data());

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void InfiniteGridRenderer::calculateFrustumCorners(const glm::mat4& inv_viewproj,
                                                       glm::vec3& near_origin, glm::vec3& near_x, glm::vec3& near_y,
                                                       glm::vec3& far_origin, glm::vec3& far_x, glm::vec3& far_y) {
        // Transform NDC corners to world space
        auto unproject = [&inv_viewproj](float x, float y, float z) -> glm::vec3 {
            glm::vec4 p = inv_viewproj * glm::vec4(x, y, z, 1.0f);
            return glm::vec3(p) / p.w;
        };

        // Near plane corners
        glm::vec3 near_bl = unproject(-1.0f, -1.0f, -1.0f);  // Bottom-left
        glm::vec3 near_br = unproject( 1.0f, -1.0f, -1.0f);  // Bottom-right
        glm::vec3 near_tl = unproject(-1.0f,  1.0f, -1.0f);  // Top-left

        // Far plane corners
        glm::vec3 far_bl = unproject(-1.0f, -1.0f, 1.0f);
        glm::vec3 far_br = unproject( 1.0f, -1.0f, 1.0f);
        glm::vec3 far_tl = unproject(-1.0f,  1.0f, 1.0f);

        // Calculate origins and axes
        near_origin = near_bl;
        near_x = near_br - near_bl;
        near_y = near_tl - near_bl;

        far_origin = far_bl;
        far_x = far_br - far_bl;
        far_y = far_tl - far_bl;
    }

    void InfiniteGridRenderer::render(const Viewport& viewport, GridPlane plane) {
        if (!initialized_) {
            std::cerr << "InfiniteGridRenderer not initialized!" << std::endl;
            return;
        }

        // Calculate view and projection matrices
        glm::mat4 view = glm::mat4(viewport.camera.R);
        view[3] = glm::vec4(-viewport.camera.R * viewport.camera.t, 1.0f);

        float aspect = static_cast<float>(viewport.windowSize.x) / viewport.windowSize.y;
        float fov = glm::radians(75.0f);  // Default FOV, should match your camera settings
        glm::mat4 projection = glm::perspective(fov, aspect, 0.01f, 10000.0f);

        glm::mat4 viewProj = projection * view;
        glm::mat4 invViewProj = glm::inverse(viewProj);

        // Calculate frustum corners
        glm::vec3 near_origin, near_x, near_y, far_origin, far_x, far_y;
        calculateFrustumCorners(invViewProj, near_origin, near_x, near_y, far_origin, far_x, far_y);

        // Get camera position
        glm::vec3 view_position = -viewport.camera.R * viewport.camera.t;

        // Save current OpenGL state
        GLboolean blend_enabled = glIsEnabled(GL_BLEND);
        GLboolean depth_test_enabled = glIsEnabled(GL_DEPTH_TEST);
        GLboolean depth_mask;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask);
        GLint blend_src, blend_dst;
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &blend_src);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &blend_dst);

        // Set our required state
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);  // We want to write depth for the grid

        // Bind shader and set uniforms
        grid_shader_->bind();

        grid_shader_->set_uniform("near_origin", near_origin);
        grid_shader_->set_uniform("near_x", near_x);
        grid_shader_->set_uniform("near_y", near_y);
        grid_shader_->set_uniform("far_origin", far_origin);
        grid_shader_->set_uniform("far_x", far_x);
        grid_shader_->set_uniform("far_y", far_y);

        grid_shader_->set_uniform("view_position", view_position);
        grid_shader_->set_uniform("matrix_viewProjection", viewProj);
        grid_shader_->set_uniform("plane", static_cast<int>(plane));

        // Bind blue noise texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, blue_noise_texture_);
        grid_shader_->set_uniform("blueNoiseTex32", 0);

        // Render the grid
        glBindVertexArray(vao_);

        // Render the grid
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glBindVertexArray(0);
        grid_shader_->unbind();

        // Restore previous OpenGL state
        if (!blend_enabled) glDisable(GL_BLEND);
        if (!depth_test_enabled) glDisable(GL_DEPTH_TEST);
        glDepthMask(depth_mask);
        glBlendFunc(blend_src, blend_dst);
    }
    }