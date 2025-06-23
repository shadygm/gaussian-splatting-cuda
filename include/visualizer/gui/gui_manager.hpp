#pragma once

#include "visualizer/gl_headers.hpp"
#include "visualizer/viewport.hpp"
#include <imgui.h>
#include <memory>
#include <string>
#include <vector>

namespace gs {

    // Forward declarations
    class GSViewer;
    class Trainer;

    // Base class for GUI panels
    class GUIPanel {
    public:
        GUIPanel(const std::string& title) : title_(title),
                                             visible_(true) {}
        virtual ~GUIPanel() = default;

        // Main render method
        virtual void render() = 0;

        // Visibility control
        bool isVisible() const { return visible_; }
        void setVisible(bool visible) { visible_ = visible; }

        // Window activity check
        bool isWindowActive() const { return window_active_; }

        // Add public getter for title
        const std::string& getTitle() const { return title_; }

    protected:
        std::string title_;
        bool visible_;
        bool window_active_ = false;
        ImGuiWindowFlags window_flags_ = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize;
    };

    // GUI Manager that handles all panels
    class GUIManager {
    public:
        GUIManager();
        ~GUIManager();

        // Initialize ImGui
        bool init(GLFWwindow* window);
        void shutdown();

        // Start new frame
        void beginFrame();

        // Render all panels
        void render();

        // End frame and render ImGui
        void endFrame();

        // Panel management
        void addPanel(std::shared_ptr<GUIPanel> panel);
        void removePanel(const std::string& title);
        std::shared_ptr<GUIPanel> getPanel(const std::string& title);

        // Check if any window is active (for input handling)
        bool isAnyWindowActive() const;

        // Global style settings
        void setStyle(const std::string& style);

        // Get number of panels
        size_t getPanelCount() const { return panels_.size(); }

    private:
        std::vector<std::shared_ptr<GUIPanel>> panels_;
        ImGuiWindowFlags default_window_flags_;
        bool initialized_ = false;
    };

} // namespace gs