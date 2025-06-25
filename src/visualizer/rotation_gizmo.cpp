#include "visualizer/rotation_gizmo.hpp"
#include "visualizer/gl_headers.hpp"
#include "visualizer/opengl_state_manager.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <iostream>
#include <vector>

namespace gs {

    RotationGizmo::RotationGizmo() {}

    RotationGizmo::~RotationGizmo() {
        if (initialized_) {
            glDeleteVertexArrays(3, vao_rings_);
            glDeleteBuffers(3, vbo_rings_);
            glDeleteVertexArrays(1, &vao_sphere_);
            glDeleteBuffers(1, &vbo_sphere_);
            glDeleteBuffers(1, &ebo_sphere_);
        }
    }

    bool RotationGizmo::init(const std::string& shader_base_path) {
        try {
            gizmo_shader_ = std::make_shared<Shader>(
                (shader_base_path + "/rotation_gizmo.vert").c_str(),
                (shader_base_path + "/rotation_gizmo.frag").c_str(),
                false);
        } catch (const std::exception& e) {
            std::cerr << "Failed to load rotation gizmo shaders: " << e.what() << std::endl;
            return false;
        }

        createGeometry();
        initialized_ = true;
        return true;
    }

    void RotationGizmo::createGeometry() {
        // Create ring geometry for each axis
        for (int axis = 0; axis < 3; ++axis) {
            std::vector<glm::vec3> vertices;

            // Generate circle vertices
            for (int i = 0; i <= num_ring_vertices_; ++i) {
                float angle = 2.0f * M_PI * i / num_ring_vertices_;
                glm::vec3 vertex(0.0f);

                if (axis == 0) { // X axis - YZ plane
                    vertex.y = cos(angle);
                    vertex.z = sin(angle);
                } else if (axis == 1) { // Y axis - XZ plane
                    vertex.x = cos(angle);
                    vertex.z = sin(angle);
                } else { // Z axis - XY plane
                    vertex.x = cos(angle);
                    vertex.y = sin(angle);
                }

                vertices.push_back(vertex);
            }

            // Create VAO and VBO
            glGenVertexArrays(1, &vao_rings_[axis]);
            glGenBuffers(1, &vbo_rings_[axis]);

            glBindVertexArray(vao_rings_[axis]);
            glBindBuffer(GL_ARRAY_BUFFER, vbo_rings_[axis]);
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3),
                         vertices.data(), GL_STATIC_DRAW);

            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

            glBindVertexArray(0);
        }

        // Create sphere for center
        std::vector<glm::vec3> sphere_vertices;
        std::vector<unsigned int> sphere_indices;

        const int slices = 16;
        const int stacks = 16;
        const float sphere_radius = 0.05f; // Smaller sphere

        for (int i = 0; i <= stacks; ++i) {
            float phi = M_PI * i / stacks;
            for (int j = 0; j <= slices; ++j) {
                float theta = 2.0f * M_PI * j / slices;

                glm::vec3 vertex(
                    sphere_radius * sin(phi) * cos(theta),
                    sphere_radius * cos(phi),
                    sphere_radius * sin(phi) * sin(theta));
                sphere_vertices.push_back(vertex);
            }
        }

        // Generate indices
        for (int i = 0; i < stacks; ++i) {
            for (int j = 0; j < slices; ++j) {
                int first = i * (slices + 1) + j;
                int second = first + slices + 1;

                sphere_indices.push_back(first);
                sphere_indices.push_back(second);
                sphere_indices.push_back(first + 1);

                sphere_indices.push_back(second);
                sphere_indices.push_back(second + 1);
                sphere_indices.push_back(first + 1);
            }
        }

        num_sphere_indices_ = sphere_indices.size();

        // Create sphere VAO
        glGenVertexArrays(1, &vao_sphere_);
        glGenBuffers(1, &vbo_sphere_);
        glGenBuffers(1, &ebo_sphere_);

        glBindVertexArray(vao_sphere_);

        glBindBuffer(GL_ARRAY_BUFFER, vbo_sphere_);
        glBufferData(GL_ARRAY_BUFFER, sphere_vertices.size() * sizeof(glm::vec3),
                     sphere_vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_sphere_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sphere_indices.size() * sizeof(unsigned int),
                     sphere_indices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

        glBindVertexArray(0);
    }

    void RotationGizmo::render(const Viewport& viewport) {
        if (!initialized_ || !visible_)
            return;

        OpenGLStateManager::StateGuard guard(getGLStateManager());

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glLineWidth(4.0f); // Thicker lines for visibility

        gizmo_shader_->bind();

        glm::mat4 view = viewport.getViewMatrix();
        glm::mat4 projection = viewport.getProjectionMatrix();

        // Calculate dynamic scale based on distance to camera
        glm::vec3 cam_pos = viewport.getCameraPosition();
        float distance_to_cam = glm::length(cam_pos - position_);

        // Keep gizmo size relatively constant on screen
        float screen_space_scale = distance_to_cam * 0.1f; // Adjust this factor as needed
        float dynamic_scale = radius_ * screen_space_scale / 10.0f;
        dynamic_scale = glm::clamp(dynamic_scale, radius_ * 0.8f, radius_ * 1.2f); // Small variation

        glm::mat4 model = glm::translate(glm::mat4(1.0f), position_) *
                          glm::scale(glm::mat4(1.0f), glm::vec3(dynamic_scale));

        gizmo_shader_->set_uniform("model", model);
        gizmo_shader_->set_uniform("view", view);
        gizmo_shader_->set_uniform("projection", projection);

        // Render rings with better visibility
        for (int axis = 0; axis < 3; ++axis) {
            glm::vec3 color = (rotating_ && active_axis_ == static_cast<Axis>(axis)) ? hover_color_ : axis_colors_[axis];

            // Make inactive rings slightly transparent
            float alpha = (rotating_ && active_axis_ != static_cast<Axis>(axis)) ? 0.5f : 1.0f;

            gizmo_shader_->set_uniform("color", glm::vec4(color, alpha));
            gizmo_shader_->set_uniform("isActive", rotating_ && active_axis_ == static_cast<Axis>(axis));

            glBindVertexArray(vao_rings_[axis]);
            glDrawArrays(GL_LINE_STRIP, 0, num_ring_vertices_ + 1);
        }

        // Render center sphere
        gizmo_shader_->set_uniform("color", glm::vec4(0.7f, 0.7f, 0.7f, 1.0f));
        gizmo_shader_->set_uniform("isActive", false);

        glBindVertexArray(vao_sphere_);
        glDrawElements(GL_TRIANGLES, num_sphere_indices_, GL_UNSIGNED_INT, 0);

        glBindVertexArray(0);
        gizmo_shader_->unbind();
    }

    RotationGizmo::Axis RotationGizmo::hitTest(const Viewport& viewport, float screen_x, float screen_y) {
        if (!visible_)
            return Axis::NONE;

        // Convert screen coordinates to ray
        glm::vec2 ndc;
        ndc.x = (2.0f * screen_x) / viewport.windowSize.x - 1.0f;
        ndc.y = 1.0f - (2.0f * screen_y) / viewport.windowSize.y;

        glm::mat4 view = viewport.getViewMatrix();
        glm::mat4 projection = viewport.getProjectionMatrix();
        glm::mat4 invVP = glm::inverse(projection * view);

        glm::vec4 near_point = invVP * glm::vec4(ndc.x, ndc.y, -1.0f, 1.0f);
        glm::vec4 far_point = invVP * glm::vec4(ndc.x, ndc.y, 1.0f, 1.0f);

        near_point /= near_point.w;
        far_point /= far_point.w;

        glm::vec3 ray_origin = glm::vec3(near_point);
        glm::vec3 ray_dir = glm::normalize(glm::vec3(far_point - near_point));

        // Calculate dynamic scale (same as in render)
        glm::vec3 cam_pos = viewport.getCameraPosition();
        float distance_to_cam = glm::length(cam_pos - position_);
        float dynamic_scale = radius_ * (distance_to_cam / 10.0f);
        dynamic_scale = glm::clamp(dynamic_scale, radius_ * 0.5f, radius_ * 2.0f);

        // Test intersection with each ring
        const float hit_threshold = ring_thickness_ * dynamic_scale * 3.0f; // More generous hit area

        for (int axis = 0; axis < 3; ++axis) {
            // Sample points on the ring and find closest distance to ray
            for (int i = 0; i < num_ring_vertices_; ++i) {
                float angle = 2.0f * M_PI * i / num_ring_vertices_;
                glm::vec3 point = position_;

                if (axis == 0) { // X axis
                    point.y += dynamic_scale * cos(angle);
                    point.z += dynamic_scale * sin(angle);
                } else if (axis == 1) { // Y axis
                    point.x += dynamic_scale * cos(angle);
                    point.z += dynamic_scale * sin(angle);
                } else { // Z axis
                    point.x += dynamic_scale * cos(angle);
                    point.y += dynamic_scale * sin(angle);
                }

                // Point-to-ray distance
                glm::vec3 to_point = point - ray_origin;
                float t = glm::dot(to_point, ray_dir);
                if (t < 0)
                    continue; // Point is behind ray origin

                glm::vec3 closest = ray_origin + t * ray_dir;
                float dist = glm::length(point - closest);

                if (dist < hit_threshold) {
                    return static_cast<Axis>(axis);
                }
            }
        }

        return Axis::NONE;
    }

    void RotationGizmo::startRotation(Axis axis, float screen_x, float screen_y, const Viewport& viewport) {
        if (axis == Axis::NONE)
            return;

        rotating_ = true;
        active_axis_ = axis;
        start_angle_ = angleFromScreenPos(screen_x, screen_y, axis, viewport);
        current_angle_ = 0.0f;
    }

    void RotationGizmo::updateRotation(float screen_x, float screen_y, const Viewport& viewport) {
        if (!rotating_)
            return;

        float angle = angleFromScreenPos(screen_x, screen_y, active_axis_, viewport);
        float delta_angle = angle - start_angle_;

        // Handle wrap around
        if (delta_angle > M_PI)
            delta_angle -= 2.0f * M_PI;
        if (delta_angle < -M_PI)
            delta_angle += 2.0f * M_PI;

        current_angle_ = delta_angle;

        // Create rotation quaternion for current drag
        glm::vec3 axis_vec(0.0f);
        if (active_axis_ == Axis::X)
            axis_vec = glm::vec3(1, 0, 0);
        else if (active_axis_ == Axis::Y)
            axis_vec = glm::vec3(0, 1, 0);
        else if (active_axis_ == Axis::Z)
            axis_vec = glm::vec3(0, 0, 1);

        glm::quat drag_rotation = glm::angleAxis(current_angle_, axis_vec);

        // Create transform that rotates around the gizmo position (scene center)
        glm::mat4 translate_to_origin = glm::translate(glm::mat4(1.0f), -position_);
        glm::mat4 rotate = glm::mat4_cast(rotation_quat_ * drag_rotation);
        glm::mat4 translate_back = glm::translate(glm::mat4(1.0f), position_);

        transform_matrix_ = translate_back * rotate * translate_to_origin;

        // Debug output
        static int debug_count = 0;
        if (debug_count++ % 10 == 0) { // Print every 10th update
            std::cout << "Gizmo rotating - angle: " << current_angle_
                      << ", axis: " << static_cast<int>(active_axis_)
                      << ", center: (" << position_.x << ", " << position_.y << ", " << position_.z << ")" << std::endl;
        }
    }

    void RotationGizmo::endRotation() {
        if (!rotating_)
            return;

        // Apply current rotation to accumulated rotation
        glm::vec3 axis_vec(0.0f);
        if (active_axis_ == Axis::X)
            axis_vec = glm::vec3(1, 0, 0);
        else if (active_axis_ == Axis::Y)
            axis_vec = glm::vec3(0, 1, 0);
        else if (active_axis_ == Axis::Z)
            axis_vec = glm::vec3(0, 0, 1);

        glm::quat drag_rotation = glm::angleAxis(current_angle_, axis_vec);
        rotation_quat_ = rotation_quat_ * drag_rotation;

        // Update the final transform matrix
        glm::mat4 translate_to_origin = glm::translate(glm::mat4(1.0f), -position_);
        glm::mat4 rotate = glm::mat4_cast(rotation_quat_);
        glm::mat4 translate_back = glm::translate(glm::mat4(1.0f), position_);

        transform_matrix_ = translate_back * rotate * translate_to_origin;

        std::cout << "Gizmo rotation ended. Final transform:\n";
        for (int i = 0; i < 4; ++i) {
            std::cout << transform_matrix_[i][0] << " " << transform_matrix_[i][1]
                      << " " << transform_matrix_[i][2] << " " << transform_matrix_[i][3] << "\n";
        }

        rotating_ = false;
        active_axis_ = Axis::NONE;
        current_angle_ = 0.0f;
    }

    float RotationGizmo::angleFromScreenPos(float x, float y, Axis axis, const Viewport& viewport) {
        // Project gizmo center to screen
        glm::vec2 center_screen = projectToScreen(position_, viewport);

        // Calculate angle from screen position
        glm::vec2 delta = glm::vec2(x, y) - center_screen;
        return atan2(delta.y, delta.x);
    }

    glm::vec2 RotationGizmo::projectToScreen(const glm::vec3& world_pos, const Viewport& viewport) {
        glm::mat4 view = viewport.getViewMatrix();
        glm::mat4 projection = viewport.getProjectionMatrix();
        glm::mat4 mvp = projection * view;

        glm::vec4 clip_pos = mvp * glm::vec4(world_pos, 1.0f);
        glm::vec3 ndc = glm::vec3(clip_pos) / clip_pos.w;

        glm::vec2 screen;
        screen.x = (ndc.x + 1.0f) * 0.5f * viewport.windowSize.x;
        screen.y = (1.0f - ndc.y) * 0.5f * viewport.windowSize.y;

        return screen;
    }

} // namespace gs