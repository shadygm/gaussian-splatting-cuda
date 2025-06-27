#include "visualizer/translation_gizmo.hpp"
#include "visualizer/gl_headers.hpp"
#include "visualizer/opengl_state_manager.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vector>

namespace gs {

    TranslationGizmo::TranslationGizmo() {}

    TranslationGizmo::~TranslationGizmo() {
        if (initialized_) {
            glDeleteVertexArrays(3, vao_arrows_);
            glDeleteBuffers(3, vbo_arrows_);
            glDeleteVertexArrays(3, vao_planes_);
            glDeleteBuffers(3, vbo_planes_);
            glDeleteVertexArrays(1, &vao_sphere_);
            glDeleteBuffers(1, &vbo_sphere_);
            glDeleteBuffers(1, &ebo_sphere_);
        }
    }

    bool TranslationGizmo::init(const std::string& shader_base_path) {
        try {
            // Can reuse the rotation gizmo shader
            gizmo_shader_ = std::make_shared<Shader>(
                (shader_base_path + "/rotation_gizmo.vert").c_str(),
                (shader_base_path + "/rotation_gizmo.frag").c_str(),
                false);
        } catch (const std::exception& e) {
            std::cerr << "Failed to load translation gizmo shaders: " << e.what() << std::endl;
            return false;
        }

        createGeometry();
        initialized_ = true;
        return true;
    }

    void TranslationGizmo::createGeometry() {
        // Create arrow geometry for each axis
        for (int axis = 0; axis < 3; ++axis) {
            std::vector<glm::vec3> vertices;

            // Arrow shaft (cylinder)
            const int segments = 16;
            for (int i = 0; i <= segments; ++i) {
                float angle = 2.0f * M_PI * i / segments;
                float x = arrow_radius_ * cos(angle);
                float y = arrow_radius_ * sin(angle);

                glm::vec3 bottom(0.0f), top(0.0f);

                if (axis == 0) { // X axis
                    bottom = glm::vec3(0.0f, x, y);
                    top = glm::vec3(arrow_length_ - cone_height_, x, y);
                } else if (axis == 1) { // Y axis
                    bottom = glm::vec3(x, 0.0f, y);
                    top = glm::vec3(x, arrow_length_ - cone_height_, y);
                } else { // Z axis
                    bottom = glm::vec3(x, y, 0.0f);
                    top = glm::vec3(x, y, arrow_length_ - cone_height_);
                }

                vertices.push_back(bottom);
                vertices.push_back(top);
            }

            // Arrow head (cone)
            glm::vec3 tip(0.0f);
            glm::vec3 base_center(0.0f);

            if (axis == 0) {
                tip = glm::vec3(arrow_length_, 0.0f, 0.0f);
                base_center = glm::vec3(arrow_length_ - cone_height_, 0.0f, 0.0f);
            } else if (axis == 1) {
                tip = glm::vec3(0.0f, arrow_length_, 0.0f);
                base_center = glm::vec3(0.0f, arrow_length_ - cone_height_, 0.0f);
            } else {
                tip = glm::vec3(0.0f, 0.0f, arrow_length_);
                base_center = glm::vec3(0.0f, 0.0f, arrow_length_ - cone_height_);
            }

            int base_start = vertices.size();
            for (int i = 0; i <= segments; ++i) {
                float angle = 2.0f * M_PI * i / segments;
                float x = cone_radius_ * cos(angle);
                float y = cone_radius_ * sin(angle);

                glm::vec3 base_point;
                if (axis == 0) {
                    base_point = base_center + glm::vec3(0.0f, x, y);
                } else if (axis == 1) {
                    base_point = base_center + glm::vec3(x, 0.0f, y);
                } else {
                    base_point = base_center + glm::vec3(x, y, 0.0f);
                }

                vertices.push_back(base_point);
                vertices.push_back(tip);
            }

            num_arrow_vertices_[axis] = vertices.size();

            // Create VAO and VBO
            glGenVertexArrays(1, &vao_arrows_[axis]);
            glGenBuffers(1, &vbo_arrows_[axis]);

            glBindVertexArray(vao_arrows_[axis]);
            glBindBuffer(GL_ARRAY_BUFFER, vbo_arrows_[axis]);
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3),
                         vertices.data(), GL_STATIC_DRAW);

            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

            glBindVertexArray(0);
        }

        // Create plane geometry for 2D constraints
        for (int plane = 0; plane < 3; ++plane) {
            std::vector<glm::vec3> vertices;

            // Small square at the origin
            glm::vec3 v1, v2, v3, v4;
            float s = plane_size_;

            if (plane == 0) { // XY plane
                v1 = glm::vec3(0, 0, 0);
                v2 = glm::vec3(s, 0, 0);
                v3 = glm::vec3(s, s, 0);
                v4 = glm::vec3(0, s, 0);
            } else if (plane == 1) { // XZ plane
                v1 = glm::vec3(0, 0, 0);
                v2 = glm::vec3(s, 0, 0);
                v3 = glm::vec3(s, 0, s);
                v4 = glm::vec3(0, 0, s);
            } else { // YZ plane
                v1 = glm::vec3(0, 0, 0);
                v2 = glm::vec3(0, s, 0);
                v3 = glm::vec3(0, s, s);
                v4 = glm::vec3(0, 0, s);
            }

            // Two triangles
            vertices.push_back(v1);
            vertices.push_back(v2);
            vertices.push_back(v3);

            vertices.push_back(v1);
            vertices.push_back(v3);
            vertices.push_back(v4);

            // Create VAO and VBO
            glGenVertexArrays(1, &vao_planes_[plane]);
            glGenBuffers(1, &vbo_planes_[plane]);

            glBindVertexArray(vao_planes_[plane]);
            glBindBuffer(GL_ARRAY_BUFFER, vbo_planes_[plane]);
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3),
                         vertices.data(), GL_STATIC_DRAW);

            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

            glBindVertexArray(0);
        }

        // Create center sphere for free movement
        std::vector<glm::vec3> sphere_vertices;
        std::vector<unsigned int> sphere_indices;

        const int slices = 16;
        const int stacks = 16;
        const float sphere_radius = 0.1f;

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

    void TranslationGizmo::render(const Viewport& viewport) {
        if (!initialized_ || !visible_)
            return;

        OpenGLStateManager::StateGuard guard(getGLStateManager());

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        gizmo_shader_->bind();

        glm::mat4 view = viewport.getViewMatrix();
        glm::mat4 projection = viewport.getProjectionMatrix();

        // Calculate dynamic scale based on distance to camera
        glm::vec3 current_pos = getPosition();
        glm::vec3 cam_pos = viewport.getCameraPosition();
        float distance_to_cam = glm::length(cam_pos - current_pos);

        // Keep gizmo size relatively constant on screen - INCREASED SCALE
        float screen_space_scale = distance_to_cam * 0.15f; // Increased from 0.1f to 0.15f
        float dynamic_scale = scale_ * screen_space_scale / 10.0f;
        dynamic_scale = glm::clamp(dynamic_scale, scale_ * 1.2f, scale_ * 2.0f); // Increased minimum

        glm::mat4 model = glm::translate(glm::mat4(1.0f), current_pos) *
                          glm::scale(glm::mat4(1.0f), glm::vec3(dynamic_scale));

        gizmo_shader_->set_uniform("model", model);
        gizmo_shader_->set_uniform("view", view);
        gizmo_shader_->set_uniform("projection", projection);

        // Render arrows with thicker lines
        glLineWidth(6.0f); // Increased from default
        for (int axis = 0; axis < 3; ++axis) {
            glm::vec3 color = (translating_ && active_axis_ == static_cast<Axis>(axis))
                                  ? hover_color_
                                  : axis_colors_[axis];

            float alpha = (translating_ && active_axis_ != static_cast<Axis>(axis)) ? 0.3f : 1.0f;

            gizmo_shader_->set_uniform("color", glm::vec4(color, alpha));
            gizmo_shader_->set_uniform("isActive", translating_ && active_axis_ == static_cast<Axis>(axis));

            glBindVertexArray(vao_arrows_[axis]);
            glDrawArrays(GL_LINES, 0, num_arrow_vertices_[axis]);
        }

        // Render constraint planes
        glDisable(GL_CULL_FACE);
        for (int plane = 0; plane < 3; ++plane) {
            Axis plane_axis = static_cast<Axis>(plane + 3); // XY, XZ, YZ
            glm::vec3 color = (translating_ && active_axis_ == plane_axis)
                                  ? hover_color_
                                  : plane_colors_[plane];

            float alpha = 0.4f; // Increased base alpha from 0.3f
            if (translating_ && active_axis_ == plane_axis)
                alpha = 0.6f;

            gizmo_shader_->set_uniform("color", glm::vec4(color, alpha));
            gizmo_shader_->set_uniform("isActive", translating_ && active_axis_ == plane_axis);

            glBindVertexArray(vao_planes_[plane]);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
        glEnable(GL_CULL_FACE);

        // Render center sphere
        glm::vec3 sphere_color = (translating_ && active_axis_ == Axis::XYZ)
                                     ? hover_color_
                                     : center_color_;

        gizmo_shader_->set_uniform("color", glm::vec4(sphere_color, 1.0f));
        gizmo_shader_->set_uniform("isActive", translating_ && active_axis_ == Axis::XYZ);

        glBindVertexArray(vao_sphere_);
        glDrawElements(GL_TRIANGLES, num_sphere_indices_, GL_UNSIGNED_INT, 0);

        glBindVertexArray(0);
        gizmo_shader_->unbind();
    }

    TranslationGizmo::Axis TranslationGizmo::hitTest(const Viewport& viewport, float screen_x, float screen_y) {
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

        // Calculate dynamic scale - MATCH RENDER SCALE
        glm::vec3 current_pos = getPosition();
        glm::vec3 cam_pos = viewport.getCameraPosition();
        float distance_to_cam = glm::length(cam_pos - current_pos);
        float dynamic_scale = scale_ * (distance_to_cam * 0.15f / 10.0f); // Match render scale
        dynamic_scale = glm::clamp(dynamic_scale, scale_ * 1.2f, scale_ * 2.0f);

        float closest_dist = std::numeric_limits<float>::max();
        Axis closest_axis = Axis::NONE;

        // Test center sphere first (highest priority) - INCREASED HIT RADIUS
        {
            glm::vec3 to_sphere = current_pos - ray_origin;
            float t = glm::dot(to_sphere, ray_dir);
            glm::vec3 closest_point = ray_origin + t * ray_dir;
            float dist = glm::length(closest_point - current_pos);

            if (dist < 0.15f * dynamic_scale) { // Increased from 0.1f
                return Axis::XYZ;
            }
        }

        // Test constraint planes - INCREASED HIT AREA
        float plane_threshold = plane_size_ * dynamic_scale * 1.5f; // Increased by 50%
        for (int plane = 0; plane < 3; ++plane) {
            glm::vec3 plane_normal;
            if (plane == 0)
                plane_normal = glm::vec3(0, 0, 1); // XY plane
            else if (plane == 1)
                plane_normal = glm::vec3(0, 1, 0); // XZ plane
            else
                plane_normal = glm::vec3(1, 0, 0); // YZ plane

            float denom = glm::dot(ray_dir, plane_normal);
            if (abs(denom) > 0.0001f) {
                float t = glm::dot(current_pos - ray_origin, plane_normal) / denom;
                if (t > 0) {
                    glm::vec3 hit_point = ray_origin + t * ray_dir;
                    glm::vec3 local_hit = hit_point - current_pos;

                    bool in_bounds = false;
                    if (plane == 0) { // XY
                        in_bounds = local_hit.x >= -plane_threshold * 0.1f && local_hit.x <= plane_threshold &&
                                    local_hit.y >= -plane_threshold * 0.1f && local_hit.y <= plane_threshold;
                    } else if (plane == 1) { // XZ
                        in_bounds = local_hit.x >= -plane_threshold * 0.1f && local_hit.x <= plane_threshold &&
                                    local_hit.z >= -plane_threshold * 0.1f && local_hit.z <= plane_threshold;
                    } else { // YZ
                        in_bounds = local_hit.y >= -plane_threshold * 0.1f && local_hit.y <= plane_threshold &&
                                    local_hit.z >= -plane_threshold * 0.1f && local_hit.z <= plane_threshold;
                    }

                    if (in_bounds) {
                        float dist = glm::length(hit_point - cam_pos);
                        if (dist < closest_dist) {
                            closest_dist = dist;
                            closest_axis = static_cast<Axis>(plane + 3);
                        }
                    }
                }
            }
        }

        // Test arrows (axes) - MUCH MORE GENEROUS HIT DETECTION
        float arrow_threshold = 0.2f * dynamic_scale; // Increased from 0.1f
        for (int axis = 0; axis < 3; ++axis) {
            glm::vec3 axis_dir(0.0f);
            axis_dir[axis] = 1.0f;

            // Test multiple points along the arrow for better hit detection
            const int num_samples = 20;
            for (int i = 0; i < num_samples; ++i) {
                float s = (arrow_length_ * dynamic_scale * i) / num_samples;
                glm::vec3 point_on_axis = current_pos + s * axis_dir;

                // Find closest point on ray to this point on axis
                glm::vec3 w = point_on_axis - ray_origin;
                float t = glm::dot(w, ray_dir);
                if (t < 0)
                    continue;

                glm::vec3 point_on_ray = ray_origin + t * ray_dir;
                float dist = glm::length(point_on_axis - point_on_ray);

                if (dist < arrow_threshold) {
                    float camera_dist = glm::length(point_on_ray - cam_pos);
                    if (camera_dist < closest_dist) {
                        closest_dist = camera_dist;
                        closest_axis = static_cast<Axis>(axis);
                        break; // Found a hit on this axis
                    }
                }
            }
        }

        return closest_axis;
    }

    // -----------------------------------------------------------------------------
    //  TranslationGizmo::startTranslation ­– robust drag‑plane setup
    // -----------------------------------------------------------------------------
    void TranslationGizmo::startTranslation(Axis axis,
                                            float screen_x,
                                            float screen_y,
                                            const Viewport& viewport) {
        if (axis == Axis::NONE)
            return;

        translating_ = true;
        active_axis_ = axis;
        current_translation_ = glm::vec3(0.0f);

        const glm::vec3 gizmo_pos = getPosition(); // world
        const glm::vec3 cam_pos = viewport.getCameraPosition();
        const glm::mat4 view = viewport.getViewMatrix();

        // world‑space camera basis vectors
        const glm::vec3 cam_right = glm::vec3(view[0][0], view[1][0], view[2][0]);
        const glm::vec3 cam_up = glm::vec3(view[0][1], view[1][1], view[2][1]);

        // -------------------------------------------------------------------------
        // 1. centre‑sphere (free) -> remember screen coords only
        // -------------------------------------------------------------------------
        if (axis == Axis::XYZ) {
            start_world_pos_ = {screen_x, screen_y, 0.0f};
            return;
        }

        // -------------------------------------------------------------------------
        // 2. build a *drag plane* that is:
        //      – orthogonal to the chosen axis
        //      – as facing as possible towards the camera
        // -------------------------------------------------------------------------
        glm::vec3 axis_dir(0.0f);
        if (axis == Axis::X)
            axis_dir.x = 1.0f;
        if (axis == Axis::Y)
            axis_dir.y = 1.0f;
        if (axis == Axis::Z)
            axis_dir.z = 1.0f;

        glm::vec3 n1 = glm::cross(axis_dir, cam_right);
        glm::vec3 n2 = glm::cross(axis_dir, cam_up);

        // if either candidate is degenerate, fall back to the other
        auto len2 = [](const glm::vec3& v) { return glm::dot(v, v); };
        if (len2(n1) < 1e-8f)
            n1 = n2;
        if (len2(n2) < 1e-8f)
            n2 = n1;

        // choose the normal that faces the camera most (bigger abs(dot))
        const glm::vec3 cam_dir = glm::normalize(cam_pos - gizmo_pos);
        drag_plane_normal_ =
            (std::abs(glm::dot(glm::normalize(n1), cam_dir)) >
             std::abs(glm::dot(glm::normalize(n2), cam_dir)))
                ? glm::normalize(n1)
                : glm::normalize(n2);

        drag_plane_distance_ = glm::dot(drag_plane_normal_, gizmo_pos);

        // for plane grips (XY/XZ/YZ) normals are trivial
        if (axis == Axis::XY)
            drag_plane_normal_ = glm::vec3(0, 0, 1);
        if (axis == Axis::XZ)
            drag_plane_normal_ = glm::vec3(0, 1, 0);
        if (axis == Axis::YZ)
            drag_plane_normal_ = glm::vec3(1, 0, 0);

        // guard against accidental zero
        if (len2(drag_plane_normal_) < 1e-8f)
            drag_plane_normal_ = glm::vec3(0, 1, 0);

        // -------------------------------------------------------------------------
        // 3. remember where the first click hit the plane
        // -------------------------------------------------------------------------
        start_world_pos_ = projectToPlane(screen_x, screen_y, viewport, axis);
    }

    // -----------------------------------------------------------------------------
    //  TranslationGizmo::updateTranslation – fixed arrow direction
    // -----------------------------------------------------------------------------
    void TranslationGizmo::updateTranslation(float screen_x,
                                             float screen_y,
                                             const Viewport& viewport) {
        if (!translating_)
            return;

        // -------------------------------------------------------------------------
        // 0) Camera helpers
        // -------------------------------------------------------------------------
        const glm::vec3 cam_pos = viewport.getCameraPosition();
        const glm::vec3 gizmo_pos = getPosition();
        const float cam_dist = glm::length(cam_pos - gizmo_pos);

        const glm::mat4 view = viewport.getViewMatrix();
        const glm::vec3 cam_right = glm::normalize(glm::vec3(view[0])); // +X
        const glm::vec3 cam_up = glm::normalize(glm::vec3(view[1]));    // +Y

        const float PIXEL2WORLD = cam_dist * 0.05f; // screen‑pixel → world units

        // -------------------------------------------------------------------------
        // 1) Free‑move sphere (XYZ)
        // -------------------------------------------------------------------------
        if (active_axis_ == Axis::XYZ) {
            const glm::vec2 delta_px =
                glm::vec2(screen_x, screen_y) -
                glm::vec2(start_world_pos_.x, start_world_pos_.y);

            current_translation_ =
                cam_right * (delta_px.x) * PIXEL2WORLD - cam_up * (delta_px.y) * PIXEL2WORLD;
            return;
        }

        // -------------------------------------------------------------------------
        // 2) Project current mouse ray onto drag‑plane
        // -------------------------------------------------------------------------
        const glm::vec3 hit_world =
            projectToPlane(screen_x, screen_y, viewport, active_axis_);

        glm::vec3 delta = hit_world - start_world_pos_; // raw world delta

        // -------------------------------------------------------------------------
        // 3a) *** SINGLE‑AXIS handles (FIXED SIGN) ***
        // -------------------------------------------------------------------------
        if (active_axis_ == Axis::X ||
            active_axis_ == Axis::Y ||
            active_axis_ == Axis::Z) {
            glm::vec3 axis(0.0f);
            axis[int(active_axis_)] = 1.0f;

            /*  Negate the signed distance so that a positive drag on the handle
                results in a positive scene translation along the same axis          */
            const float signed_distance = -glm::dot(delta, axis);

            current_translation_ = axis * signed_distance * 2.0f; // 2× speed‑up
            return;
        }

        // -------------------------------------------------------------------------
        // 3b) Plane handles (XY / XZ / YZ) – unchanged
        // -------------------------------------------------------------------------
        switch (active_axis_) {
        case Axis::XY: delta.z = 0.0f; break;
        case Axis::XZ: delta.y = 0.0f; break;
        case Axis::YZ: delta.x = 0.0f; break;
        default: /* never here */ break;
        }

        current_translation_ = delta * 2.0f; // 2× speed‑up
    }

    void TranslationGizmo::endTranslation() {
        if (!translating_)
            return;

        // IMPORTANT: Add current translation to accumulated BEFORE clearing it!
        accumulated_translation_ += current_translation_;

        std::cout << "Translation ended. Current translation was: ("
                  << current_translation_.x << ", "
                  << current_translation_.y << ", "
                  << current_translation_.z << ")" << std::endl;
        std::cout << "New accumulated translation: ("
                  << accumulated_translation_.x << ", "
                  << accumulated_translation_.y << ", "
                  << accumulated_translation_.z << ")" << std::endl;

        // NOW clear the current translation
        current_translation_ = glm::vec3(0.0f);
        translating_ = false;
        active_axis_ = Axis::NONE;
    }

    glm::mat4 TranslationGizmo::getTransformMatrix() const {
        glm::vec3 total_translation = accumulated_translation_;
        if (translating_) {
            total_translation += current_translation_;
        }
        return glm::translate(glm::mat4(1.0f), total_translation);
    }

    glm::vec3 TranslationGizmo::projectToPlane(float screen_x, float screen_y,
                                               const Viewport& viewport, Axis axis) {
        // Convert screen to ray
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

        // Debug ray
        static int debug_count = 0;
        bool should_debug = (debug_count++ % 20 == 0);

        if (should_debug) {
            std::cout << "projectToPlane debug:" << std::endl;
            std::cout << "  Screen: (" << screen_x << ", " << screen_y << ")" << std::endl;
            std::cout << "  NDC: (" << ndc.x << ", " << ndc.y << ")" << std::endl;
            std::cout << "  Ray origin: (" << ray_origin.x << ", " << ray_origin.y << ", " << ray_origin.z << ")" << std::endl;
            std::cout << "  Ray dir: (" << ray_dir.x << ", " << ray_dir.y << ", " << ray_dir.z << ")" << std::endl;
            std::cout << "  Plane normal: (" << drag_plane_normal_.x << ", " << drag_plane_normal_.y << ", " << drag_plane_normal_.z << ")" << std::endl;
            std::cout << "  Plane distance: " << drag_plane_distance_ << std::endl;
        }

        // Intersect with drag plane
        float denom = glm::dot(ray_dir, drag_plane_normal_);
        if (abs(denom) < 0.0001f) {
            // Ray is nearly parallel to plane - find closest point on plane
            if (should_debug) {
                std::cout << "WARNING: Ray nearly parallel to drag plane (dot=" << denom << ")" << std::endl;
            }

            // Project ray origin onto plane
            float dist_to_plane = glm::dot(ray_origin, drag_plane_normal_) - drag_plane_distance_;
            glm::vec3 closest_on_plane = ray_origin - dist_to_plane * drag_plane_normal_;

            return closest_on_plane;
        }

        float t = (drag_plane_distance_ - glm::dot(ray_origin, drag_plane_normal_)) / denom;
        glm::vec3 intersection = ray_origin + t * ray_dir;

        if (should_debug) {
            std::cout << "  Denom: " << denom << std::endl;
            std::cout << "  Intersection t: " << t << std::endl;
            std::cout << "  Intersection point: (" << intersection.x << ", " << intersection.y << ", " << intersection.z << ")" << std::endl;
        }

        return intersection;
    }

    glm::vec3 TranslationGizmo::screenToWorld(float screen_x, float screen_y, float depth,
                                              const Viewport& viewport) {
        glm::vec2 ndc;
        ndc.x = (2.0f * screen_x) / viewport.windowSize.x - 1.0f;
        ndc.y = 1.0f - (2.0f * screen_y) / viewport.windowSize.y;

        glm::mat4 view = viewport.getViewMatrix();
        glm::mat4 projection = viewport.getProjectionMatrix();
        glm::mat4 invVP = glm::inverse(projection * view);

        glm::vec4 world_pos = invVP * glm::vec4(ndc.x, ndc.y, depth, 1.0f);
        world_pos /= world_pos.w;

        return glm::vec3(world_pos);
    }

    float TranslationGizmo::getScreenDepth(const glm::vec3& world_pos, const Viewport& viewport) {
        glm::mat4 view = viewport.getViewMatrix();
        glm::mat4 projection = viewport.getProjectionMatrix();
        glm::mat4 vp = projection * view;

        glm::vec4 clip_pos = vp * glm::vec4(world_pos, 1.0f);
        return clip_pos.z / clip_pos.w;
    }

} // namespace gs