#pragma once

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/transform.hpp>
#include <iostream>

class Viewport {
public:
    glm::ivec2 windowSize;
    glm::ivec2 frameBufferSize;

    // Camera state - simple and direct
    float azimuth = -45.0f;                         // Horizontal rotation angle
    float elevation = -30.0f;                       // Vertical rotation angle
    float distance = 10.0f;                         // Distance from origin
    glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f); // Look-at target (world origin)

    // Mouse state
    glm::vec2 lastMousePos;
    bool mouseInitialized = false;

    // Control parameters
    float orbitSensitivity = 0.3f;
    float zoomSensitivity = 0.1f;
    float panSensitivity = 0.002f;

    // Legacy camera member for compatibility
    struct CameraMotion {
        bool mouseInitialized = false;
        bool ortho = false;
        float sceneRadius = 1.0f;
        float minZoom = 0.1f;
        float maxZoom = 1000.0f;

        void initScreenPos(const glm::vec2& pos) {}
        void rotate(const glm::vec2& pos) {}
        void translate(const glm::vec2& pos) {}
        void zoom(float delta) {}
        void reset() {}
        void alignToAxis(char axis, bool positive) {}
        float getDistance() const { return 10.0f; }
        glm::vec3 getFocalPoint() const { return glm::vec3(0.0f); }
        void updateCamera() {}
        void update(float deltaTime) {}
    } camera;

    Viewport(size_t width = 1280, size_t height = 720) {
        windowSize = glm::ivec2(width, height);
        frameBufferSize = windowSize;

        std::cout << "Viewport initialized:" << std::endl;
        std::cout << "  Azimuth: " << azimuth << ", Elevation: " << elevation << std::endl;
        std::cout << "  Distance: " << distance << std::endl;
        std::cout << "  Target: " << target.x << ", " << target.y << ", " << target.z << std::endl;
    }

    // Get camera position in world space
    glm::vec3 getCameraPosition() const {
        float azimRad = glm::radians(azimuth);
        float elevRad = glm::radians(elevation);

        float cosElev = std::cos(elevRad);
        float sinElev = std::sin(elevRad);
        float cosAzim = std::cos(azimRad);
        float sinAzim = std::sin(azimRad);

        // Standard spherical coordinates
        glm::vec3 offset(
            cosElev * sinAzim,
            sinElev,
            cosElev * cosAzim);

        return target + offset * distance;
    }

    // Get view matrix (world to camera transformation)
    glm::mat4 getViewMatrix() const {
        glm::vec3 eye = getCameraPosition();
        glm::vec3 up(0, 1, 0);

        // Handle looking straight up/down
        if (std::abs(std::cos(glm::radians(elevation))) < 0.01f) {
            float azimRad = glm::radians(azimuth);
            up = glm::vec3(-std::sin(azimRad) * std::sin(glm::radians(elevation)), 0,
                           -std::cos(azimRad) * std::sin(glm::radians(elevation)));
        }

        return glm::lookAt(eye, target, up);
    }

    // Get projection matrix
    glm::mat4 getProjectionMatrix(float fov = 75.0f, float near = 0.1f, float far = 1000.0f) const {
        float aspect = static_cast<float>(windowSize.x) / static_cast<float>(windowSize.y);
        return glm::perspective(glm::radians(fov), aspect, near, far);
    }

    // Get combined view-projection matrix
    glm::mat4 getViewProjectionMatrix() const {
        return getProjectionMatrix() * getViewMatrix();
    }

    // Mouse controls
    void initScreenPos(const glm::vec2& pos) {
        lastMousePos = pos;
        mouseInitialized = true;
        camera.mouseInitialized = true;
    }

    // Left mouse drag - orbit around target
    void rotate(const glm::vec2& pos) {
        if (!mouseInitialized) {
            initScreenPos(pos);
            return;
        }

        glm::vec2 delta = pos - lastMousePos;

        std::cout << "Viewport::rotate - delta: (" << delta.x << ", " << delta.y << ")" << std::endl;
        std::cout << "Before: azimuth=" << azimuth << ", elevation=" << elevation << std::endl;

        azimuth -= delta.x * orbitSensitivity;
        elevation += delta.y * orbitSensitivity;

        // Wrap azimuth
        while (azimuth > 360.0f)
            azimuth -= 360.0f;
        while (azimuth < 0.0f)
            azimuth += 360.0f;

        // Clamp elevation
        elevation = glm::clamp(elevation, -89.0f, 89.0f);

        std::cout << "After: azimuth=" << azimuth << ", elevation=" << elevation << std::endl;

        lastMousePos = pos;
    }

    // Right mouse drag - pan the target
    void translate(const glm::vec2& pos) {
        if (!mouseInitialized) {
            initScreenPos(pos);
            return;
        }

        glm::vec2 delta = pos - lastMousePos;

        // Get camera right and up vectors
        glm::mat4 view = getViewMatrix();
        glm::vec3 right = glm::vec3(view[0][0], view[1][0], view[2][0]);
        glm::vec3 up = glm::vec3(view[0][1], view[1][1], view[2][1]);

        float panScale = distance * panSensitivity;
        target += -right * delta.x * panScale + up * delta.y * panScale;

        lastMousePos = pos;
    }

    // Mouse wheel - zoom
    void zoom(float delta) {
        float zoomFactor = std::exp(-delta * zoomSensitivity);
        distance *= zoomFactor;
        distance = glm::clamp(distance, 0.1f, 1000.0f);
    }

    // Reset camera
    void reset() {
        azimuth = -45.0f;
        elevation = -30.0f;
        distance = 10.0f;
        target = glm::vec3(0.0f, 0.0f, 0.0f);
        mouseInitialized = false;
        camera.mouseInitialized = false;

        std::cout << "Camera reset to default" << std::endl;
    }

    // Align to axis
    void alignToAxis(char axis, bool positive) {
        target = glm::vec3(0.0f, 0.0f, 0.0f); // Look at origin

        switch (axis) {
        case 'x':
            azimuth = positive ? 90.0f : -90.0f;
            elevation = 0.0f;
            break;
        case 'y':
            azimuth = 0.0f;
            elevation = positive ? 90.0f : -90.0f;
            break;
        case 'z':
            azimuth = positive ? 180.0f : 0.0f;
            elevation = 0.0f;
            break;
        }
        camera.ortho = true;
    }

    // For compatibility with existing code
    glm::mat3 getRotationMatrix() const {
        glm::vec3 pos = getCameraPosition();
        glm::vec3 forward = glm::normalize(target - pos);
        glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
        glm::vec3 up = glm::normalize(glm::cross(right, forward));

        return glm::mat3(right, up, -forward);
    }

    glm::vec3 getTranslation() const {
        return getCameraPosition();
    }

    // Get world-to-camera rotation matrix (for Gaussian Splatting compatibility)
    glm::mat3 getWorldToCameraRotation() const {
        // The stored R is camera-to-world, so transpose it
        return glm::transpose(getRotationMatrix());
    }

    // Get world-to-camera translation (for Gaussian Splatting compatibility)
    glm::vec3 getWorldToCameraTranslation() const {
        // t_w2c = -R_w2c * t_c2w
        return -glm::transpose(getRotationMatrix()) * getCameraPosition();
    }

    // Get current camera state
    glm::vec3 getFocalPoint() const {
        return target;
    }

    float getAzimuth() const {
        return azimuth;
    }

    float getElevation() const {
        return elevation;
    }

    float getDistance() const {
        return distance;
    }

    // Check if in orthographic mode
    bool isOrtho() const {
        return camera.ortho;
    }

    // Dummy update for compatibility
    void update() {}

    // Grid plane detection
    int getOrthoGridPlane() const {
        glm::vec3 forward = glm::normalize(target - getCameraPosition());

        float dotX = std::abs(forward.x);
        float dotY = std::abs(forward.y);
        float dotZ = std::abs(forward.z);

        if (dotX > dotY && dotX > dotZ) {
            return 0; // YZ plane
        } else if (dotY > dotX && dotY > dotZ) {
            return 1; // XZ plane
        } else {
            return 2; // XY plane
        }
    }
};