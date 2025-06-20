#pragma once

#include "visualizer/shader.hpp"
#include "visualizer/viewport.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace gs {

    class ViewCubeRenderer {
    public:
        ViewCubeRenderer();
        ~ViewCubeRenderer();

        // Initialize the renderer with shader paths
        bool init(const std::string& shader_base_path);

        // Render the view cube at the specified screen position
        void render(const Viewport& viewport, float x, float y, float size);

        // Check if a screen position hits the view cube
        // Returns: -1 if no hit, 0-5 for faces (±X, ±Y, ±Z), 6-17 for edges, 18-25 for corners
        int hitTest(const Viewport& viewport, float screen_x, float screen_y, float cube_x, float cube_y, float size);

        // Get the rotation for a specific face/edge/corner
        glm::mat3 getRotationForElement(int element_id);

    private:
        struct Vertex {
            glm::vec3 position;
            glm::vec3 normal;
            glm::vec2 texcoord;
            int face_id; // Which face this vertex belongs to
        };

        void createCubeGeometry();
        void createAxisGeometry();
        glm::mat4 getViewMatrix(const glm::mat3& rotation, float distance);

    private:
        std::shared_ptr<Shader> cube_shader_;
        std::shared_ptr<Shader> axis_shader_;

        GLuint cube_vao_, cube_vbo_, cube_ebo_;
        GLuint axis_vao_, axis_vbo_;

        GLuint cube_texture_;

        int num_cube_indices_;
        int num_axis_vertices_;

        bool initialized_ = false;

        // Face colors
        const glm::vec3 face_colors_[6] = {
            glm::vec3(1.0f, 0.3f, 0.3f), // +X (red)
            glm::vec3(0.8f, 0.2f, 0.2f), // -X (dark red)
            glm::vec3(0.3f, 1.0f, 0.3f), // +Y (green)
            glm::vec3(0.2f, 0.8f, 0.2f), // -Y (dark green)
            glm::vec3(0.3f, 0.3f, 1.0f), // +Z (blue)
            glm::vec3(0.2f, 0.2f, 0.8f)  // -Z (dark blue)
        };
    };

} // namespace gs
