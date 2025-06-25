#pragma once

#include "visualizer/shader.hpp"
#include "visualizer/viewport.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>

namespace gs {

    class RotationGizmo {
    public:
        enum class Axis {
            NONE = -1,
            X = 0,
            Y = 1,
            Z = 2
        };

        RotationGizmo();
        ~RotationGizmo();

        bool init(const std::string& shader_base_path);
        void render(const Viewport& viewport);

        // Interaction
        Axis hitTest(const Viewport& viewport, float screen_x, float screen_y);
        void startRotation(Axis axis, float screen_x, float screen_y, const Viewport& viewport);
        void updateRotation(float screen_x, float screen_y, const Viewport& viewport);
        void endRotation();

        // Get the accumulated rotation
        glm::mat4 getTransformMatrix() const { return transform_matrix_; }
        glm::quat getRotationQuaternion() const { return rotation_quat_; }

        // Control
        void setPosition(const glm::vec3& pos) { position_ = pos; }
        void setRadius(float radius) { radius_ = radius; }
        void setVisible(bool visible) { visible_ = visible; }
        bool isVisible() const { return visible_; }
        bool isRotating() const { return rotating_; }

    private:
        void createGeometry();
        glm::vec2 projectToScreen(const glm::vec3& world_pos, const Viewport& viewport);
        float angleFromScreenPos(float x, float y, Axis axis, const Viewport& viewport);

        std::shared_ptr<Shader> gizmo_shader_;

        // Geometry
        GLuint vao_rings_[3]; // One for each axis
        GLuint vbo_rings_[3];
        GLuint vao_sphere_;
        GLuint vbo_sphere_;
        GLuint ebo_sphere_;
        int num_ring_vertices_ = 64;
        int num_sphere_indices_ = 0;

        // State
        bool visible_ = true;
        bool rotating_ = false;
        Axis active_axis_ = Axis::NONE;

        // Rotation state
        float start_angle_ = 0.0f;
        float current_angle_ = 0.0f;
        glm::quat rotation_quat_{1.0f, 0.0f, 0.0f, 0.0f};
        glm::mat4 transform_matrix_{1.0f};

        // Gizmo properties
        glm::vec3 position_{0.0f};
        float radius_ = 1.0f;
        float ring_thickness_ = 0.05f;

        // Colors
        const glm::vec3 axis_colors_[3] = {
            glm::vec3(1.0f, 0.2f, 0.2f), // X - Red
            glm::vec3(0.2f, 1.0f, 0.2f), // Y - Green
            glm::vec3(0.2f, 0.2f, 1.0f)  // Z - Blue
        };

        const glm::vec3 hover_color_ = glm::vec3(1.0f, 1.0f, 0.2f);

        bool initialized_ = false;
    };

} // namespace gs
