#pragma once

#include "visualizer/gl_headers.hpp"
#include "visualizer/viewport.hpp"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace gs {

    class InputHandler {
    public:
        // Key binding structure
        struct KeyBinding {
            int key;
            int mods = 0;
            std::function<void()> action;
            std::string description;
        };

        // Mouse event callbacks
        using MouseButtonCallback = std::function<bool(int button, int action, double x, double y)>;
        using MouseMoveCallback = std::function<void(double x, double y, double dx, double dy)>;
        using ScrollCallback = std::function<void(double offset)>;

        InputHandler(GLFWwindow* window, Viewport* viewport);
        ~InputHandler();

        // Key bindings
        void addKeyBinding(int key, std::function<void()> action,
                           const std::string& description, int mods = 0);
        void removeKeyBinding(int key, int mods = 0);

        // Mouse callbacks - return true if handled
        void addMouseButtonCallback(int button, MouseButtonCallback callback);
        void setMouseMoveCallback(MouseMoveCallback callback);
        void setScrollCallback(ScrollCallback callback);

        // Special callbacks for specific features
        void setViewCubeHitTest(std::function<int(double, double)> hitTest) {
            view_cube_hit_test_ = hitTest;
        }

        // Check if GUI is capturing input
        void setGUIActiveCheck(std::function<bool()> check) {
            gui_active_check_ = check;
        }

        void handleMouseButton(int button, int action, double x, double y);
        void handleMouseMove(double x, double y);
        void handleScroll(double offset);
        void handleKey(int key, int scancode, int action, int mods);

        // Get key binding descriptions for help display
        std::vector<std::pair<std::string, std::string>> getKeyBindings() const;

        // Default camera controls
        void setupDefaultCameraControls();

    private:
        static void mouseButtonCallbackStatic(GLFWwindow* window, int button, int action, int mods);
        static void cursorPosCallbackStatic(GLFWwindow* window, double x, double y);
        static void scrollCallbackStatic(GLFWwindow* window, double xoffset, double yoffset);
        static void keyCallbackStatic(GLFWwindow* window, int key, int scancode, int action, int mods);

        GLFWwindow* window_;
        Viewport* viewport_;

        // Callbacks
        std::unordered_map<int, MouseButtonCallback> mouse_button_callbacks_;
        MouseMoveCallback mouse_move_callback_;
        ScrollCallback scroll_callback_;
        std::function<int(double, double)> view_cube_hit_test_;

        // Key bindings
        std::unordered_map<uint32_t, KeyBinding> key_bindings_;

        // GUI check
        std::function<bool()> gui_active_check_;

        // Mouse state
        double last_x_ = 0.0;
        double last_y_ = 0.0;
        bool dragging_ = false;
        int drag_button_ = -1;

        // Static instance for callbacks
        static InputHandler* instance_;

        // Helper to create key hash
        uint32_t makeKeyHash(int key, int mods) const {
            return (static_cast<uint32_t>(mods) << 16) | static_cast<uint32_t>(key);
        }
    };

} // namespace gs
