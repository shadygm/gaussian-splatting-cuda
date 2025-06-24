#include "visualizer/view_cube_renderer.hpp"
#include "visualizer/gl_headers.hpp"
#include "visualizer/opengl_state_manager.hpp"
#include <cstring>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

namespace gs {

    ViewCubeRenderer::ViewCubeRenderer() {
    }

    ViewCubeRenderer::~ViewCubeRenderer() {
        if (initialized_) {
            glDeleteVertexArrays(1, &cube_vao_);
            glDeleteBuffers(1, &cube_vbo_);
            glDeleteBuffers(1, &cube_ebo_);
            glDeleteVertexArrays(1, &axis_vao_);
            glDeleteBuffers(1, &axis_vbo_);
            glDeleteTextures(1, &cube_texture_);
        }
    }

    bool ViewCubeRenderer::init(const std::string& shader_base_path) {
        // Load shaders
        try {
            cube_shader_ = std::make_shared<Shader>(
                (shader_base_path + "/view_cube.vert").c_str(),
                (shader_base_path + "/view_cube.frag").c_str(),
                false);

            axis_shader_ = std::make_shared<Shader>(
                (shader_base_path + "/view_cube_axis.vert").c_str(),
                (shader_base_path + "/view_cube_axis.frag").c_str(),
                false);
        } catch (const std::exception& e) {
            std::cerr << "Failed to load view cube shaders: " << e.what() << std::endl;
            return false;
        }

        createCubeGeometry();
        createAxisGeometry();

        // Create texture with face labels
        glGenTextures(1, &cube_texture_);
        glBindTexture(GL_TEXTURE_2D, cube_texture_);

        // Create a simple white texture for now (you can add labels later)
        unsigned char white_pixel[] = {255, 255, 255, 255};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white_pixel);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        initialized_ = true;
        return true;
    }

    void ViewCubeRenderer::createCubeGeometry() {
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;

        // Define cube vertices for each face
        const float s = 0.5f;

        // Face vertices (6 faces * 4 vertices each)
        const glm::vec3 positions[8] = {
            glm::vec3(-s, -s, -s), glm::vec3(s, -s, -s),
            glm::vec3(s, s, -s), glm::vec3(-s, s, -s),
            glm::vec3(-s, -s, s), glm::vec3(s, -s, s),
            glm::vec3(s, s, s), glm::vec3(-s, s, s)};

        // Define faces with their vertices and normals
        struct Face {
            int indices[4];
            glm::vec3 normal;
            int id;
        };

        const Face faces[6] = {
            {{1, 5, 6, 2}, glm::vec3(1, 0, 0), 0},  // +X
            {{4, 0, 3, 7}, glm::vec3(-1, 0, 0), 1}, // -X
            {{7, 3, 2, 6}, glm::vec3(0, 1, 0), 2},  // +Y
            {{4, 5, 1, 0}, glm::vec3(0, -1, 0), 3}, // -Y
            {{5, 4, 7, 6}, glm::vec3(0, 0, 1), 4},  // +Z
            {{0, 1, 2, 3}, glm::vec3(0, 0, -1), 5}  // -Z
        };

        // Create vertices for each face
        for (const auto& face : faces) {
            int base_index = vertices.size();

            // Add 4 vertices for this face
            for (int i = 0; i < 4; i++) {
                Vertex v;
                v.position = positions[face.indices[i]];
                v.normal = face.normal;
                v.face_id = face.id;

                // Simple texture coordinates
                v.texcoord = glm::vec2(
                    (i == 0 || i == 3) ? 0.0f : 1.0f,
                    (i == 0 || i == 1) ? 0.0f : 1.0f);

                vertices.push_back(v);
            }

            // Add two triangles for this face
            indices.push_back(base_index + 0);
            indices.push_back(base_index + 1);
            indices.push_back(base_index + 2);

            indices.push_back(base_index + 0);
            indices.push_back(base_index + 2);
            indices.push_back(base_index + 3);
        }

        num_cube_indices_ = indices.size();

        // Create VAO and VBO
        glGenVertexArrays(1, &cube_vao_);
        glGenBuffers(1, &cube_vbo_);
        glGenBuffers(1, &cube_ebo_);

        glBindVertexArray(cube_vao_);
        glBindBuffer(GL_ARRAY_BUFFER, cube_vbo_);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cube_ebo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        // Set up vertex attributes
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texcoord));

        glEnableVertexAttribArray(3);
        glVertexAttribIPointer(3, 1, GL_INT, sizeof(Vertex), (void*)offsetof(Vertex, face_id));

        glBindVertexArray(0);
    }

    void ViewCubeRenderer::createAxisGeometry() {
        std::vector<glm::vec3> vertices;

        // X axis (red)
        vertices.push_back(glm::vec3(0, 0, 0));
        vertices.push_back(glm::vec3(1, 0, 0));

        // Y axis (green)
        vertices.push_back(glm::vec3(0, 0, 0));
        vertices.push_back(glm::vec3(0, 1, 0));

        // Z axis (blue)
        vertices.push_back(glm::vec3(0, 0, 0));
        vertices.push_back(glm::vec3(0, 0, 1));

        num_axis_vertices_ = vertices.size();

        // Create VAO and VBO
        glGenVertexArrays(1, &axis_vao_);
        glGenBuffers(1, &axis_vbo_);

        glBindVertexArray(axis_vao_);
        glBindBuffer(GL_ARRAY_BUFFER, axis_vbo_);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

        glBindVertexArray(0);
    }

    glm::mat4 ViewCubeRenderer::getViewMatrix(const glm::mat3& rotation, float distance) {
        // Create view matrix that looks at the cube from the given rotation
        glm::vec3 eye = rotation * glm::vec3(0, 0, distance);
        glm::vec3 center(0, 0, 0);
        glm::vec3 up = rotation * glm::vec3(0, 1, 0);

        return glm::lookAt(eye, center, up);
    }

    void ViewCubeRenderer::render(const Viewport& viewport, float x, float y, float size) {
        if (!initialized_)
            return;

        // Save current state
        OpenGLStateManager::StateGuard guard(getGLStateManager());

        // Set up viewport for cube rendering
        glViewport(x - size / 2, y - size / 2, size, size);

        // Use preset for view cube
        getGLStateManager().setForViewCube();

        // Clear depth in the cube area
        glScissor(x - size / 2, y - size / 2, size, size);
        glEnable(GL_SCISSOR_TEST);
        glClear(GL_DEPTH_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);

        // Use viewport rotation directly without conversion
        glm::mat4 projection = glm::perspective(glm::radians(50.0f), 1.0f, 0.1f, 10.0f);
        glm::mat4 view = getViewMatrix(viewport.getRotationMatrix(), 3.0f);
        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 mvp = projection * view * model;

        // Render cube faces
        cube_shader_->bind();
        cube_shader_->set_uniform("mvpMatrix", mvp);
        cube_shader_->set_uniform("modelMatrix", model);

        // Pass face colors as uniforms
        for (int i = 0; i < 6; i++) {
            std::string uniform_name = "faceColors[" + std::to_string(i) + "]";
            cube_shader_->set_uniform(uniform_name, face_colors_[i]);
        }

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, cube_texture_);
        cube_shader_->set_uniform("faceTexture", 0);

        glBindVertexArray(cube_vao_);
        glDrawElements(GL_TRIANGLES, num_cube_indices_, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        cube_shader_->unbind();

        // Render axes
        glDisable(GL_DEPTH_TEST); // Axes render on top
        glLineWidth(3.0f);

        axis_shader_->bind();
        axis_shader_->set_uniform("mvpMatrix", mvp);

        glBindVertexArray(axis_vao_);

        // Draw each axis with its color
        const glm::vec3 axis_colors[3] = {
            glm::vec3(1, 0, 0), // X - red
            glm::vec3(0, 1, 0), // Y - green
            glm::vec3(0, 0, 1)  // Z - blue
        };

        for (int i = 0; i < 3; i++) {
            axis_shader_->set_uniform("axisColor", axis_colors[i]);
            glDrawArrays(GL_LINES, i * 2, 2);
        }

        glBindVertexArray(0);
        axis_shader_->unbind();

        // State automatically restored by guard destructor
    }

    int ViewCubeRenderer::hitTest(const Viewport& viewport, float screen_x, float screen_y,
                                  float cube_x, float cube_y, float size) {
        // Check if click is within cube bounds
        float dx = screen_x - cube_x;
        float dy = screen_y - cube_y;

        if (std::abs(dx) > size / 2 || std::abs(dy) > size / 2) {
            return -1; // Outside cube area
        }

        // Convert to normalized device coordinates within the cube viewport
        float ndx = (dx / (size / 2));
        float ndy = -(dy / (size / 2)); // Flip Y

        // Set up the same view as used for rendering
        glm::mat4 projection = glm::perspective(glm::radians(50.0f), 1.0f, 0.1f, 10.0f);
        glm::mat4 view = getViewMatrix(viewport.getRotationMatrix(), 3.0f);
        glm::mat4 mvp = projection * view;
        glm::mat4 mvp_inv = glm::inverse(mvp);

        // Create ray from camera
        glm::vec4 near_point = mvp_inv * glm::vec4(ndx, ndy, -1.0f, 1.0f);
        glm::vec4 far_point = mvp_inv * glm::vec4(ndx, ndy, 1.0f, 1.0f);

        near_point /= near_point.w;
        far_point /= far_point.w;

        glm::vec3 ray_origin = glm::vec3(near_point);
        glm::vec3 ray_dir = glm::normalize(glm::vec3(far_point - near_point));

        // Test intersection with each face
        float closest_t = std::numeric_limits<float>::max();
        int closest_face = -1;

        // Face normals and distances
        const glm::vec3 face_normals[6] = {
            glm::vec3(1, 0, 0), glm::vec3(-1, 0, 0),
            glm::vec3(0, 1, 0), glm::vec3(0, -1, 0),
            glm::vec3(0, 0, 1), glm::vec3(0, 0, -1)};

        const float cube_half_size = 0.5f;

        for (int i = 0; i < 6; i++) {
            float denom = glm::dot(ray_dir, face_normals[i]);
            if (std::abs(denom) > 0.0001f) {
                float t = (cube_half_size - glm::dot(ray_origin, face_normals[i])) / denom;
                if (t > 0 && t < closest_t) {
                    glm::vec3 hit_point = ray_origin + t * ray_dir;

                    // Check if hit point is within face bounds
                    bool within_bounds = true;
                    for (int j = 0; j < 3; j++) {
                        if (j != i / 2) { // Check other axes
                            if (std::abs(hit_point[j]) > cube_half_size) {
                                within_bounds = false;
                                break;
                            }
                        }
                    }

                    if (within_bounds) {
                        closest_t = t;
                        closest_face = i;
                    }
                }
            }
        }

        return closest_face;
    }

    glm::mat3 ViewCubeRenderer::getRotationForElement(int element_id) {
        // Return rotation matrix to align camera with the selected element
        glm::mat3 rotation(1.0f);

        switch (element_id) {
        case 0: // +X face
            rotation = glm::mat3(
                0, 0, 1,
                0, 1, 0,
                -1, 0, 0);
            break;
        case 1: // -X face
            rotation = glm::mat3(
                0, 0, -1,
                0, 1, 0,
                1, 0, 0);
            break;
        case 2: // +Y face
            rotation = glm::mat3(
                1, 0, 0,
                0, 0, -1,
                0, 1, 0);
            break;
        case 3: // -Y face
            rotation = glm::mat3(
                1, 0, 0,
                0, 0, 1,
                0, -1, 0);
            break;
        case 4: // +Z face
            rotation = glm::mat3(
                1, 0, 0,
                0, 1, 0,
                0, 0, 1);
            break;
        case 5: // -Z face
            rotation = glm::mat3(
                -1, 0, 0,
                0, 1, 0,
                0, 0, -1);
            break;
        }

        return rotation;
    }

} // namespace gs
