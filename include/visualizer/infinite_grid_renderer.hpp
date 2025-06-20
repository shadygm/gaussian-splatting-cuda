#pragma once

#include "visualizer/shader.hpp"
#include "visualizer/viewport.hpp"
#include <glm/glm.hpp>
#include <memory>

namespace gs {

    class InfiniteGridRenderer {
    public:
        enum class GridPlane {
            YZ = 0, // X plane (YZ grid)
            XZ = 1, // Y plane (XZ grid) - typically the ground plane
            XY = 2  // Z plane (XY grid)
        };

        InfiniteGridRenderer();
        ~InfiniteGridRenderer();

        // Initialize the renderer with shader paths
        bool init(const std::string& shader_base_path);

        // Render the infinite grid
        void render(const Viewport& viewport, GridPlane plane = GridPlane::XZ);

        // Set grid parameters
        void setOpacity(float opacity) { opacity_ = glm::clamp(opacity, 0.0f, 1.0f); }
        void setFadeDistance(float near_dist, float far_dist) {
            fade_start_ = near_dist;
            fade_end_ = far_dist;
        }

    private:
        // Calculate frustum corners for near/far planes
        void calculateFrustumCorners(const glm::mat4& inv_viewproj,
                                     glm::vec3& near_origin, glm::vec3& near_x, glm::vec3& near_y,
                                     glm::vec3& far_origin, glm::vec3& far_x, glm::vec3& far_y);

        // Create blue noise texture for dithering
        void createBlueNoiseTexture();

    private:
        std::shared_ptr<Shader> grid_shader_;
        GLuint vao_;
        GLuint vbo_;
        GLuint blue_noise_texture_;

        float opacity_ = 1.0f;
        float fade_start_ = 1000.0f;
        float fade_end_ = 5000.0f;

        bool initialized_ = false;
    };

} // namespace gs
