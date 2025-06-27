#include "visualizer/input_handler.hpp"
#include <iostream>

namespace gs {

    InputHandler::InputHandler(GLFWwindow* window, Viewport* viewport)
        : window_(window),
          viewport_(viewport) {

        // Don't set GLFW callbacks here - ViewerBase handles that

        // Setup default controls
        setupDefaultCameraControls();
    }

    InputHandler::~InputHandler() {
        // No longer managing instance pointer
    }

    void InputHandler::setupDefaultCameraControls() {
        // Default key bindings
        addKeyBinding(
            GLFW_KEY_F, [this]() {
                viewport_->reset();
            },
            "Focus on world origin");

        addKeyBinding(
            GLFW_KEY_H, [this]() {
                viewport_->target = glm::vec3(0.0f, 0.0f, 0.0f);
                viewport_->azimuth = -135.0f;
                viewport_->elevation = -60.0f;
                viewport_->distance = 10.0f;
                std::cout << "Camera set to home view at world origin" << std::endl;
            },
            "Home view (look down at origin)");

        // Simplified mouse button handling
        mouse_button_callbacks_.clear();

        // Scroll for zoom
        setScrollCallback([this](double offset) {
            float delta = static_cast<float>(offset);
            if (std::abs(delta) > 1.0e-2f) {
                viewport_->zoom(delta);
            }
        });
    }

    void InputHandler::addKeyBinding(int key, std::function<void()> action,
                                     const std::string& description, int mods) {
        uint32_t hash = makeKeyHash(key, mods);
        key_bindings_[hash] = {key, mods, action, description};
    }

    void InputHandler::removeKeyBinding(int key, int mods) {
        uint32_t hash = makeKeyHash(key, mods);
        key_bindings_.erase(hash);
    }

    void InputHandler::addMouseButtonCallback(int button, MouseButtonCallback callback) {
        mouse_button_callbacks_[button] = callback;
    }

    void InputHandler::setMouseMoveCallback(MouseMoveCallback callback) {
        mouse_move_callback_ = callback;
    }

    void InputHandler::setScrollCallback(ScrollCallback callback) {
        scroll_callback_ = callback;
    }

    std::vector<std::pair<std::string, std::string>> InputHandler::getKeyBindings() const {
        std::vector<std::pair<std::string, std::string>> bindings;

        for (const auto& [hash, binding] : key_bindings_) {
            std::string key_str;

            // Convert key to string
            switch (binding.key) {
            case GLFW_KEY_F: key_str = "F"; break;
            case GLFW_KEY_G: key_str = "G"; break;
            case GLFW_KEY_H: key_str = "H"; break;
            case GLFW_KEY_R: key_str = "R"; break;
            case GLFW_KEY_C: key_str = "C"; break;
            case GLFW_KEY_ESCAPE: key_str = "ESC"; break;
            case GLFW_KEY_LEFT: key_str = "Left Arrow"; break;
            case GLFW_KEY_RIGHT: key_str = "Right Arrow"; break;
            default:
                if (binding.key >= GLFW_KEY_A && binding.key <= GLFW_KEY_Z) {
                    key_str = std::string(1, 'A' + (binding.key - GLFW_KEY_A));
                } else {
                    key_str = "Key " + std::to_string(binding.key);
                }
            }

            // Add modifiers
            if (binding.mods & GLFW_MOD_CONTROL)
                key_str = "Ctrl+" + key_str;
            if (binding.mods & GLFW_MOD_SHIFT)
                key_str = "Shift+" + key_str;
            if (binding.mods & GLFW_MOD_ALT)
                key_str = "Alt+" + key_str;

            bindings.push_back({key_str, binding.description});
        }

        // Add mouse controls
        bindings.push_back({"Left Mouse", "Orbit camera / Rotate gizmo"});
        bindings.push_back({"Right Mouse", "Pan camera"});
        bindings.push_back({"Scroll", "Zoom camera"});

        return bindings;
    }

    // Handle methods (no longer checking GUI - that's done in ViewerBase)
    void InputHandler::handleMouseButton(int button, int action, double x, double y) {
        std::cout << "InputHandler::handleMouseButton - button=" << button
                  << ", action=" << action << ", pos=(" << x << ", " << y << ")" << std::endl;

        // First, call any registered callbacks for this button
        auto it = mouse_button_callbacks_.find(button);
        if (it != mouse_button_callbacks_.end() && it->second) {
            bool handled = it->second(button, action, x, y);
            if (handled) {
                return; // Callback handled the event
            }
        }

        // Default handling if no callback handled it
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            if (action == GLFW_PRESS) {
                // Check gizmo first
                if (gizmo_hit_test_) {
                    int hit = gizmo_hit_test_(x, y);
                    if (hit >= 0) {
                        gizmo_dragging_ = true;
                        return;
                    }
                }

                // Check view cube
                if (view_cube_hit_test_) {
                    int hit = view_cube_hit_test_(x, y);
                    if (hit >= 0) {
                        switch (hit) {
                        case 0: viewport_->alignToAxis('x', true); break;
                        case 1: viewport_->alignToAxis('x', false); break;
                        case 2: viewport_->alignToAxis('y', true); break;
                        case 3: viewport_->alignToAxis('y', false); break;
                        case 4: viewport_->alignToAxis('z', true); break;
                        case 5: viewport_->alignToAxis('z', false); break;
                        }
                        return;
                    }
                }

                // Start orbit
                viewport_->initScreenPos(glm::vec2(x, y));
                dragging_ = true;
                drag_button_ = GLFW_MOUSE_BUTTON_LEFT;
            } else if (action == GLFW_RELEASE) {
                dragging_ = false;
                drag_button_ = -1;
                viewport_->mouseInitialized = false;
                gizmo_dragging_ = false;
            }
        } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            if (action == GLFW_PRESS) {
                viewport_->initScreenPos(glm::vec2(x, y));
                dragging_ = true;
                drag_button_ = GLFW_MOUSE_BUTTON_RIGHT;
            } else if (action == GLFW_RELEASE) {
                dragging_ = false;
                drag_button_ = -1;
                viewport_->mouseInitialized = false;
            }
        }
    }

    void InputHandler::handleMouseMove(double x, double y) {
        if (!dragging_ && !gizmo_dragging_)
            return;

        glm::vec2 currentPos(x, y);

        if (gizmo_dragging_) {
            // Let viewer handle gizmo
            if (mouse_move_callback_) {
                mouse_move_callback_(x, y, 0, 0);
            }
        } else if (dragging_) {
            if (drag_button_ == GLFW_MOUSE_BUTTON_LEFT) {
                viewport_->rotate(currentPos);
            } else if (drag_button_ == GLFW_MOUSE_BUTTON_RIGHT) {
                viewport_->translate(currentPos);
            }
        }
    }

    void InputHandler::handleScroll(double offset) {
        if (scroll_callback_) {
            scroll_callback_(offset);
        }
    }

    void InputHandler::handleKey(int key, int scancode, int action, int mods) {
        if (action == GLFW_PRESS || action == GLFW_REPEAT) {
            uint32_t hash = makeKeyHash(key, mods);
            auto it = key_bindings_.find(hash);
            if (it != key_bindings_.end()) {
                it->second.action();
            }
        }
    }

} // namespace gs