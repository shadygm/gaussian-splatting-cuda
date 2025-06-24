#include "visualizer/viewer_base.hpp"
#include <imgui.h>
#include <iostream>
#include <thread>

namespace gs {

    // Static member for callbacks
    static ViewerBase* g_current_viewer = nullptr;

    ViewerBase::ViewerBase(const std::string& title, int width, int height)
        : title_(title),
          viewport_(width, height) {

        shader_path_ = std::string(PROJECT_ROOT_PATH) + "/include/visualizer/shaders/";
        last_frame_time_ = std::chrono::steady_clock::now();
    }

    ViewerBase::~ViewerBase() {
        if (g_current_viewer == this) {
            g_current_viewer = nullptr;
        }
        shutdownWindow();
    }

    // Static callback functions
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
        // Let ImGui handle it first
        if (ImGui::GetIO().WantCaptureMouse) {
            return;
        }

        if (g_current_viewer && g_current_viewer->getInputHandler()) {
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);
            g_current_viewer->getInputHandler()->handleMouseButton(button, action, xpos, ypos);
        }
    }

    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
        // Let ImGui handle it first
        if (ImGui::GetIO().WantCaptureMouse) {
            return;
        }

        if (g_current_viewer && g_current_viewer->getInputHandler()) {
            g_current_viewer->getInputHandler()->handleMouseMove(xpos, ypos);
        }
    }

    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
        // Let ImGui handle it first
        if (ImGui::GetIO().WantCaptureMouse) {
            return;
        }

        if (g_current_viewer && g_current_viewer->getInputHandler()) {
            g_current_viewer->getInputHandler()->handleScroll(yoffset);
        }
    }

    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        // Let ImGui handle it first
        if (ImGui::GetIO().WantCaptureKeyboard) {
            return;
        }

        if (g_current_viewer && g_current_viewer->getInputHandler()) {
            g_current_viewer->getInputHandler()->handleKey(key, scancode, action, mods);
        }
    }

    bool ViewerBase::initializeWindow() {
        if (!glfwInit()) {
            std::cerr << "Failed to initialize GLFW!" << std::endl;
            return false;
        }

        glfwWindowHint(GLFW_SAMPLES, 8);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_FALSE);
        glfwWindowHint(GLFW_DEPTH_BITS, 24);

        window_ = glfwCreateWindow(
            viewport_.windowSize.x,
            viewport_.windowSize.y,
            title_.c_str(), NULL, NULL);

        if (window_ == NULL) {
            std::cerr << "Failed to create GLFW window!" << std::endl;
            glfwTerminate();
            return false;
        }

        glfwMakeContextCurrent(window_);

        // Set static viewer reference
        g_current_viewer = this;

        // Set GLFW callbacks - these will check ImGui first
        glfwSetMouseButtonCallback(window_, mouseButtonCallback);
        glfwSetCursorPosCallback(window_, cursorPosCallback);
        glfwSetScrollCallback(window_, scrollCallback);
        glfwSetKeyCallback(window_, keyCallback);

        return true;
    }

    bool ViewerBase::initializeOpenGL() {
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            std::cerr << "GLAD init failed" << std::endl;
            return false;
        }

        glfwSwapInterval(1); // Enable vsync

        // Set default OpenGL state
        glEnable(GL_LINE_SMOOTH);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBlendEquation(GL_FUNC_ADD);
        glEnable(GL_PROGRAM_POINT_SIZE);

        return true;
    }

    bool ViewerBase::initializeComponents() {
        // Initialize scene renderer
        scene_renderer_ = std::make_unique<SceneRenderer>();
        if (!scene_renderer_->initialize(shader_path_)) {
            std::cerr << "Failed to initialize scene renderer" << std::endl;
            return false;
        }

        // Initialize GUI manager
        gui_manager_ = std::make_unique<GUIManager>();
        if (!gui_manager_->init(window_)) {
            std::cerr << "Failed to initialize GUI manager" << std::endl;
            return false;
        }

        // Initialize input handler
        input_handler_ = std::make_unique<InputHandler>(window_, &viewport_);

        // Remove the GUI check - we handle it in the static callbacks now
        // input_handler_->setGUIActiveCheck([this]() {
        //     return gui_manager_->isAnyWindowActive();
        // });

        // Set view cube hit test
        input_handler_->setViewCubeHitTest([this](double x, double y) {
            return scene_renderer_->hitTestViewCube(viewport_, x, y);
        });

        return true;
    }

    void ViewerBase::shutdownWindow() {
        if (gui_manager_) {
            gui_manager_->shutdown();
        }

        if (window_) {
            glfwDestroyWindow(window_);
            window_ = nullptr;
        }

        glfwTerminate();
    }

    void ViewerBase::run() {
        if (!initializeWindow()) {
            std::cerr << "Failed to initialize window" << std::endl;
            return;
        }

        if (!initializeOpenGL()) {
            std::cerr << "Failed to initialize OpenGL" << std::endl;
            shutdownWindow();
            return;
        }

        if (!initializeComponents()) {
            std::cerr << "Failed to initialize components" << std::endl;
            shutdownWindow();
            return;
        }

        // Call derived class initialization
        onInitialize();

        // Setup GUI after initialization
        setupGUI();

        initialized_ = true;

        // Main loop
        while (!glfwWindowShouldClose(window_)) {
            limitFrameRate();
            updateWindowSize();

            // Update viewport for smooth camera transitions
            viewport_.update();

            // Clear screen
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Call derived class drawing
            onDraw();

            // Render GUI
            gui_manager_->beginFrame();
            gui_manager_->render();
            gui_manager_->endFrame();

            glfwSwapBuffers(window_);
            glfwPollEvents();
        }

        onClose();
    }

    void ViewerBase::setTargetFPS(int fps) {
        target_fps_ = fps;
    }

    void ViewerBase::limitFrameRate() {
        auto now = std::chrono::steady_clock::now();
        auto frame_duration = std::chrono::duration_cast<std::chrono::microseconds>(now - last_frame_time_);

        const auto target_duration = std::chrono::microseconds(1000000 / target_fps_);

        if (frame_duration < target_duration) {
            std::this_thread::sleep_for(target_duration - frame_duration);
        }

        last_frame_time_ = std::chrono::steady_clock::now();
    }

    void ViewerBase::updateWindowSize() {
        int winW, winH, fbW, fbH;
        glfwGetWindowSize(window_, &winW, &winH);
        glfwGetFramebufferSize(window_, &fbW, &fbH);

        if (viewport_.windowSize.x != winW || viewport_.windowSize.y != winH) {
            viewport_.windowSize = glm::ivec2(winW, winH);
            viewport_.frameBufferSize = glm::ivec2(fbW, fbH);
            glViewport(0, 0, fbW, fbH);

            onResize(winW, winH);
        }
    }

} // namespace gs