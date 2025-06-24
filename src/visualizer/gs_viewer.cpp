#include "visualizer/gs_viewer.hpp"
#include <imgui.h>
#include <iostream>
#include <thread>

namespace gs {

    // Static member for callbacks
    static GSViewer* g_current_viewer = nullptr;

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

    GSViewer::GSViewer(const std::string& title, int width, int height)
        : title_(title),
          viewport_(width, height) {

        shader_path_ = std::string(PROJECT_ROOT_PATH) + "/include/visualizer/shaders/";
        last_frame_time_ = std::chrono::steady_clock::now();

        render_config_ = std::make_shared<RenderingConfig>();
        training_info_ = std::make_shared<TrainingInfo>();
        notifier_ = std::make_shared<Notifier>();

        setTargetFPS(30);

        // Default render settings
        render_settings_.show_grid = true;
        render_settings_.show_view_cube = true;
        render_settings_.show_cameras = true;
        render_settings_.grid_plane = InfiniteGridRenderer::GridPlane::XZ;

        std::cout << "GSViewer constructed" << std::endl;
    }

    GSViewer::~GSViewer() {
        if (g_current_viewer == this) {
            g_current_viewer = nullptr;
        }

        // If trainer is still running, request it to stop
        if (trainer_ && trainer_->is_running()) {
            std::cout << "Viewer closing - stopping training..." << std::endl;
            trainer_->request_stop();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::cout << "GSViewer destroyed." << std::endl;
        shutdownWindow();
    }

    bool GSViewer::initializeWindow() {
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

    bool GSViewer::initializeOpenGL() {
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

    bool GSViewer::initializeComponents() {
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

        // Set view cube hit test
        input_handler_->setViewCubeHitTest([this](double x, double y) {
            return scene_renderer_->hitTestViewCube(viewport_, x, y);
        });

        return true;
    }

    void GSViewer::shutdownWindow() {
        if (gui_manager_) {
            gui_manager_->shutdown();
        }

        if (window_) {
            glfwDestroyWindow(window_);
            window_ = nullptr;
        }

        glfwTerminate();
    }

    void GSViewer::run() {
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

    void GSViewer::setTargetFPS(int fps) {
        target_fps_ = fps;
    }

    void GSViewer::limitFrameRate() {
        auto now = std::chrono::steady_clock::now();
        auto frame_duration = std::chrono::duration_cast<std::chrono::microseconds>(now - last_frame_time_);

        const auto target_duration = std::chrono::microseconds(1000000 / target_fps_);

        if (frame_duration < target_duration) {
            std::this_thread::sleep_for(target_duration - frame_duration);
        }

        last_frame_time_ = std::chrono::steady_clock::now();
    }

    void GSViewer::updateWindowSize() {
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

    void GSViewer::setTrainer(Trainer* trainer) {
        trainer_ = trainer;

        // Setup panels if GUI manager exists
        if (getGUIManager()) {
            setupPanels();
        }
    }

    void GSViewer::setDataset(std::shared_ptr<CameraDataset> dataset) {
        dataset_ = dataset;

        // Pass cameras to renderer
        if (dataset_ && getSceneRenderer()) {
            std::vector<bool> is_test_camera(dataset_->get_cameras().size());
            for (size_t i = 0; i < dataset_->get_cameras().size(); ++i) {
                is_test_camera[i] = (i % 8) == 0; // Default test_every=8
            }
            getSceneRenderer()->setCameras(dataset_->get_cameras(), is_test_camera);
        }

        // Setup panels if GUI manager exists
        if (getGUIManager()) {
            setupPanels();
        }
    }

    void GSViewer::onInitialize() {
        std::cout << "GSViewer::onInitialize()" << std::endl;

        // Setup additional key bindings
        setupAdditionalKeyBindings();
    }

    void GSViewer::onClose() {
        std::cout << "GSViewer::onClose()" << std::endl;
    }

    void GSViewer::setupGUI() {
        if (hasTrainer() || hasDataset()) {
            setupPanels();
        }
    }

    void GSViewer::setupPanels() {
        std::cout << "GSViewer::setupPanels() called" << std::endl;

        auto gui = getGUIManager();
        if (!gui)
            return;

        // Clear existing panels
        while (gui->getPanelCount() > 0) {
            gui->removePanel("Training Control");
            gui->removePanel("Rendering Settings");
            gui->removePanel("Camera Controls");
            gui->removePanel("Visualization Settings");
            gui->removePanel("Dataset Viewer");
        }

        try {
            // Create panels based on what's available
            if (trainer_) {
                std::cout << "Creating training panels..." << std::endl;
                training_panel_ = std::make_shared<TrainingControlPanel>(trainer_, training_info_);
                render_panel_ = std::make_shared<RenderSettingsPanel>(render_config_);
                gui->addPanel(training_panel_);
                gui->addPanel(render_panel_);
            }

            // Always add camera and visualization panels
            std::cout << "Creating camera and visualization panels..." << std::endl;
            camera_panel_ = std::make_shared<CameraControlPanel>(&getViewport());
            viz_panel_ = std::make_shared<VisualizationPanel>(
                getSceneRenderer()->getGridRenderer(),
                getSceneRenderer()->getViewCubeRenderer(),
                &render_settings_.show_grid,
                &render_settings_.show_view_cube);

            gui->addPanel(camera_panel_);
            gui->addPanel(viz_panel_);

            // Add dataset viewer if dataset is available
            if (dataset_ && getSceneRenderer()->getCameraRenderer()) {
                std::cout << "Creating dataset viewer panel..." << std::endl;
                dataset_panel_ = std::make_shared<DatasetViewerPanel>(
                    dataset_,
                    getSceneRenderer()->getCameraRenderer(),
                    &getViewport());
                gui->addPanel(dataset_panel_);
            }

            std::cout << "GUI panels setup complete" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Exception during GUI panel setup: " << e.what() << std::endl;
        }
    }

    void GSViewer::setupAdditionalKeyBindings() {
        auto input = getInputHandler();
        if (!input)
            return;

        // Toggle grid
        input->addKeyBinding(
            GLFW_KEY_G, [this]() {
                render_settings_.show_grid = !render_settings_.show_grid;
            },
            "Toggle grid");

        // Toggle cameras
        input->addKeyBinding(
            GLFW_KEY_C, [this]() {
                render_settings_.show_cameras = !render_settings_.show_cameras;
            },
            "Toggle camera frustums");

        // Navigation keys for dataset
        input->addKeyBinding(
            GLFW_KEY_LEFT, [this]() {
                if (dataset_panel_) {
                    dataset_panel_->previousCamera();
                }
            },
            "Previous camera");

        input->addKeyBinding(
            GLFW_KEY_RIGHT, [this]() {
                if (dataset_panel_) {
                    dataset_panel_->nextCamera();
                }
            },
            "Next camera");

        input->addKeyBinding(
            GLFW_KEY_ESCAPE, [this]() {
                if (dataset_panel_ && dataset_panel_->shouldShowImageOverlay()) {
                    // Toggle overlay off
                    render_settings_.show_image_overlay = false;
                }
            },
            "Close image overlay");
    }

    void GSViewer::updateSceneBounds() {
        if (!trainer_ || scene_bounds_initialized_) {
            return;
        }

        if (trainer_->get_strategy().get_model().size() > 0) {
            auto& model = trainer_->get_strategy().get_model();
            auto means = model.get_means();

            if (means.size(0) > 0) {
                auto min_vals = std::get<0>(means.min(0));
                auto max_vals = std::get<0>(means.max(0));

                glm::vec3 min_point(
                    min_vals[0].item<float>(),
                    min_vals[1].item<float>(),
                    min_vals[2].item<float>());

                glm::vec3 max_point(
                    max_vals[0].item<float>(),
                    max_vals[1].item<float>(),
                    max_vals[2].item<float>());

                scene_center_ = (min_point + max_point) * 0.5f;
                scene_radius_ = glm::length(max_point - min_point) * 0.5f;

                // Clamp radius to reasonable range
                scene_radius_ = std::clamp(scene_radius_, 0.1f, 100.0f);

                scene_bounds_valid_ = true;
                scene_bounds_initialized_ = true;

                std::cout << "Scene bounds - Center: ("
                          << scene_center_.x << ", "
                          << scene_center_.y << ", "
                          << scene_center_.z << "), Radius: "
                          << scene_radius_ << std::endl;

                // Update components with scene bounds
                getViewport().camera.sceneRadius = scene_radius_;
                getViewport().camera.minZoom = scene_radius_ * 0.01f;
                getViewport().camera.maxZoom = scene_radius_ * 100.0f;

                getSceneRenderer()->updateSceneBounds(scene_center_, scene_radius_);

                if (camera_panel_) {
                    camera_panel_->setSceneBounds(scene_center_, scene_radius_);
                }

                std::cout << "Camera remains at world origin. Use mouse to navigate." << std::endl;
            }
        }
    }

    void GSViewer::handleTrainingStart() {
        if (trainer_ && training_panel_ && training_panel_->shouldStartTraining() && notifier_) {
            std::lock_guard<std::mutex> lock(notifier_->mtx);
            notifier_->ready = true;
            notifier_->cv.notify_one();
            training_panel_->resetStartTrigger();
        }
    }

    void GSViewer::onDraw() {
        // Update scene bounds if needed
        updateSceneBounds();

        auto renderer = getSceneRenderer();

        // 1. Draw grid
        renderer->renderGrid(getViewport(), render_settings_);

        // 2. Draw camera frustums
        if (render_settings_.show_cameras && dataset_panel_) {
            int highlight_idx = dataset_panel_->getCurrentCameraIndex();
            renderer->renderCameras(getViewport(), highlight_idx);
        }

        // 3. Draw splats if trainer is available
        if (trainer_) {
            // Clear depth buffer so splats render on top
            glClear(GL_DEPTH_BUFFER_BIT);
            renderer->renderSplats(getViewport(), trainer_, render_config_, splat_mutex_);
        }

        // 4. Draw view cube
        renderer->renderViewCube(getViewport(), render_settings_.show_view_cube);

        // 5. Draw image overlay if enabled
        if (render_settings_.show_image_overlay && dataset_panel_) {
            auto image = dataset_panel_->getCurrentImage();
            if (image.defined()) {
                float overlay_width = 400.0f;
                float overlay_height = overlay_width * image.size(1) / image.size(2);
                float margin = 20.0f;
                float x = getViewport().windowSize.x - overlay_width - margin;
                float y = margin;

                renderer->renderImageOverlay(getViewport(), image, x, y, overlay_width, overlay_height);
            }
        }

        // Handle training start trigger
        handleTrainingStart();
    }

} // namespace gs