#pragma once

#include "visualizer/shader.hpp"
#include "visualizer/viewport.hpp"
#include <glm/glm.hpp>
#include <memory>

namespace gs {

    class TranslationGizmo {
    public:
        enum class Axis {
            NONE = -1,
            X = 0,
            Y = 1,
            Z = 2,
            XY = 3,
            XZ = 4,
            YZ = 5,
            XYZ = 6 // Center sphere for free movement
        };

        TranslationGizmo();
        ~TranslationGizmo();

        bool init(const std::string& shader_base_path);
        void render(const Viewport& viewport);

        // Interaction
        Axis hitTest(const Viewport& viewport, float screen_x, float screen_y);
        void startTranslation(Axis axis, float screen_x, float screen_y, const Viewport& viewport);
        void updateTranslation(float screen_x, float screen_y, const Viewport& viewport);
        void endTranslation();

        // Get the accumulated translation
        glm::vec3 getTranslation() const { return accumulated_translation_; }
        glm::mat4 getTransformMatrix() const;

        // Control
        void setPosition(const glm::vec3& pos) { base_position_ = pos; }
        glm::vec3 getPosition() const { return base_position_ + accumulated_translation_; }
        void setScale(float scale) { scale_ = scale; }
        void setVisible(bool visible) { visible_ = visible; }
        bool isVisible() const { return visible_; }
        bool isTranslating() const { return translating_; }

        // Reset accumulated translation
        void reset() { accumulated_translation_ = glm::vec3(0.0f); }

    private:
        void createGeometry();
        glm::vec3 projectToPlane(float screen_x, float screen_y, const Viewport& viewport, Axis axis);
        glm::vec3 screenToWorld(float screen_x, float screen_y, float depth, const Viewport& viewport);
        float getScreenDepth(const glm::vec3& world_pos, const Viewport& viewport);

        std::shared_ptr<Shader> gizmo_shader_;

        // Geometry for arrows
        GLuint vao_arrows_[3];
        GLuint vbo_arrows_[3];
        int num_arrow_vertices_[3];

        // Geometry for planes
        GLuint vao_planes_[3];
        GLuint vbo_planes_[3];

        // Center sphere
        GLuint vao_sphere_;
        GLuint vbo_sphere_;
        GLuint ebo_sphere_;
        int num_sphere_indices_;

        // State
        bool visible_ = true;
        bool translating_ = false;
        Axis active_axis_ = Axis::NONE;

        // Translation state
        glm::vec3 start_world_pos_{0.0f};
        glm::vec3 current_translation_{0.0f};
        glm::vec3 accumulated_translation_{0.0f};
        glm::vec3 base_position_{0.0f};

        // Drag plane for constrained movement
        glm::vec3 drag_plane_normal_{0.0f};
        float drag_plane_distance_ = 0.0f;

        // Appearance
        float scale_ = 1.0f;
        float arrow_length_ = 1.0f;
        float arrow_radius_ = 0.02f;
        float cone_height_ = 0.2f;
        float cone_radius_ = 0.06f;
        float plane_size_ = 0.3f;

        // Colors
        const glm::vec3 axis_colors_[3] = {
            glm::vec3(1.0f, 0.2f, 0.2f), // X - Red
            glm::vec3(0.2f, 1.0f, 0.2f), // Y - Green
            glm::vec3(0.2f, 0.2f, 1.0f)  // Z - Blue
        };

        const glm::vec3 plane_colors_[3] = {
            glm::vec3(1.0f, 1.0f, 0.2f), // XY - Yellow
            glm::vec3(1.0f, 0.2f, 1.0f), // XZ - Magenta
            glm::vec3(0.2f, 1.0f, 1.0f)  // YZ - Cyan
        };

        const glm::vec3 hover_color_ = glm::vec3(1.0f, 1.0f, 0.2f);
        const glm::vec3 center_color_ = glm::vec3(0.8f, 0.8f, 0.8f);

        bool initialized_ = false;
    };

} // namespace gs
