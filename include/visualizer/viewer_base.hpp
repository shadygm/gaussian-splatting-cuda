#pragma once

#include "visualizer/gl_headers.hpp"
#include "visualizer/gui/gui_manager.hpp"
#include "visualizer/input_handler.hpp"
#include "visualizer/scene_renderer.hpp"
#include "visualizer/viewport.hpp"
#include <chrono>
#include <memory>
#include <string>

namespace gs {

    // Base viewer class with common functionality
    class ViewerBase {
    public:
        ViewerBase(const std::string& title, int width, int height);
        virtual ~ViewerBase();

        // Main run loop
        void run();

        // Override for custom rendering
        virtual void onDraw() = 0;

        // Optional overrides for events
        virtual void onInitialize() {}
        virtual void onResize(int width, int height) {}
        virtual void onClose() {}
        virtual void setupGUI() {}

        // Access to components
        Viewport& getViewport() { return viewport_; }
        SceneRenderer* getSceneRenderer() { return scene_renderer_.get(); }
        GUIManager* getGUIManager() { return gui_manager_.get(); }
        InputHandler* getInputHandler() { return input_handler_.get(); }

        // Window access
        GLFWwindow* getWindow() { return window_; }

    protected:
        // Window and context initialization
        bool initializeWindow();
        bool initializeOpenGL();
        bool initializeComponents();
        void shutdownWindow();

        // Frame control
        void setTargetFPS(int fps);
        void limitFrameRate();

        // Window management
        void updateWindowSize();

        GLFWwindow* window_ = nullptr;
        std::string title_;
        Viewport viewport_;

        // Components
        std::unique_ptr<SceneRenderer> scene_renderer_;
        std::unique_ptr<GUIManager> gui_manager_;
        std::unique_ptr<InputHandler> input_handler_;

        // Frame timing
        int target_fps_ = 30;
        std::chrono::steady_clock::time_point last_frame_time_;

        // Paths
        std::string shader_path_;

        // Flags
        bool initialized_ = false;
    };

} // namespace gs
