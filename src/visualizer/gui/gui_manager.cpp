#include "visualizer/gui/gui_manager.hpp"
#include <algorithm>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <filesystem>
#include <iostream>

namespace gs {

    GUIManager::GUIManager()
        : default_window_flags_(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize) {
    }

    GUIManager::~GUIManager() {
        if (initialized_) {
            shutdown();
        }
    }

    bool GUIManager::init(GLFWwindow* window) {
        if (initialized_) {
            return true;
        }

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        io.ConfigWindowsMoveFromTitleBarOnly = true;

        // Setup Platform/Renderer backends
        const char* glsl_version = "#version 430";
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init(glsl_version);

        // Set default style
        setStyle("Light");

        // Load fonts if available
        std::string font_path = std::string(PROJECT_ROOT_PATH) +
                                "/include/visualizer/assets/JetBrainsMono-Regular.ttf";
        if (std::filesystem::exists(font_path)) {
            io.Fonts->AddFontFromFileTTF(font_path.c_str(), 14.0f);
        }

        initialized_ = true;
        return true;
    }

    void GUIManager::shutdown() {
        if (!initialized_) {
            return;
        }

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        initialized_ = false;
    }

    void GUIManager::beginFrame() {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void GUIManager::render() {
        // Render all visible panels
        for (auto& panel : panels_) {
            if (panel && panel->isVisible()) {
                panel->render();
            }
        }
    }

    void GUIManager::endFrame() {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void GUIManager::addPanel(std::shared_ptr<GUIPanel> panel) {
        if (panel) {
            panels_.push_back(panel);
        }
    }

    void GUIManager::removePanel(const std::string& title) {
        panels_.erase(
            std::remove_if(panels_.begin(), panels_.end(),
                           [&title](const std::shared_ptr<GUIPanel>& panel) {
                               return panel->getTitle() == title; // Use public getter
                           }),
            panels_.end());
    }

    std::shared_ptr<GUIPanel> GUIManager::getPanel(const std::string& title) {
        auto it = std::find_if(panels_.begin(), panels_.end(),
                               [&title](const std::shared_ptr<GUIPanel>& panel) {
                                   return panel->getTitle() == title; // Use public getter
                               });

        return (it != panels_.end()) ? *it : nullptr;
    }

    bool GUIManager::isAnyWindowActive() const {
        // Check ImGui state
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) ||
            ImGui::IsAnyItemActive() ||
            ImGui::IsAnyItemHovered()) {
            return true;
        }

        // Check individual panels
        for (const auto& panel : panels_) {
            if (panel && panel->isWindowActive()) {
                return true;
            }
        }

        return false;
    }

    void GUIManager::setStyle(const std::string& style) {
        if (style == "Dark") {
            ImGui::StyleColorsDark();
        } else if (style == "Light") {
            ImGui::StyleColorsLight();
        } else if (style == "Classic") {
            ImGui::StyleColorsClassic();
        }

        // Custom style adjustments
        ImGuiStyle& imgui_style = ImGui::GetStyle();
        imgui_style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
        imgui_style.WindowPadding = ImVec2(6.0f, 6.0f);
        imgui_style.WindowRounding = 6.0f;
        imgui_style.WindowBorderSize = 0.0f;
        imgui_style.FrameRounding = 2.0f;
    }

} // namespace gs