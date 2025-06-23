#include "config.h" // Include generated config
#include "visualizer/detail.hpp"
#include <chrono>
#include <thread>

#ifdef CUDA_GL_INTEROP_ENABLED
#include "visualizer/cuda_gl_interop.hpp"
#endif

namespace gs {

    ViewerDetail* ViewerDetail::detail_ = nullptr;

    ViewerDetail::ViewerDetail(std::string title, int width, int height)
        : title_(title),
          viewport_(width, height) {
        detail_ = this;
    }

    ViewerDetail::~ViewerDetail() {
        std::cout << "Viewer destroyed." << std::endl;
    }

    bool ViewerDetail::init() {
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

        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            std::cerr << "GLAD init failed" << std::endl;
            glfwTerminate();
            return false;
        }

        glfwSwapInterval(1); // Enable vsync

        glfwSetMouseButtonCallback(window_, mouseButtonCallback);
        glfwSetCursorPosCallback(window_, cursorPosCallback);
        glfwSetScrollCallback(window_, scrollCallback);
        glfwSetKeyCallback(window_, keyCallback);

        glEnable(GL_LINE_SMOOTH);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBlendEquation(GL_FUNC_ADD);
        glEnable(GL_PROGRAM_POINT_SIZE);

        return true;
    }

    void ViewerDetail::updateWindowSize() {
        int winW, winH, fbW, fbH;
        glfwGetWindowSize(window_, &winW, &winH);
        glfwGetFramebufferSize(window_, &fbW, &fbH);
        viewport_.windowSize = glm::ivec2(winW, winH);
        viewport_.frameBufferSize = glm::ivec2(fbW, fbH);
        glViewport(0, 0, fbW, fbH);
    }

    void ViewerDetail::setFrameRate(const int fps) {
        targetFPS = fps;
        frameTime = 1000 / targetFPS;
    }

    void ViewerDetail::controlFrameRate() {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTime).count();
        if (duration < frameTime) {
            std::this_thread::sleep_for(std::chrono::milliseconds(frameTime - duration));
        }
        lastTime = std::chrono::high_resolution_clock::now();
    }

    void ViewerDetail::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
        // First check GUI
        if (detail_->gui_manager_ && detail_->gui_manager_->isAnyWindowActive()) {
            return;
        }

        if (action == GLFW_PRESS) {
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);

            // Check if click is on view cube for left button
            if (button == GLFW_MOUSE_BUTTON_LEFT && detail_->view_cube_renderer_ && detail_->show_view_cube_) {
                float margin = 20.0f;
                float size = 120.0f;
                float cube_x = detail_->viewport_.windowSize.x - margin - size / 2;
                float cube_y = detail_->viewport_.windowSize.y - margin - size / 2;

                // Convert to OpenGL coordinates (flip Y)
                float gl_y = detail_->viewport_.windowSize.y - ypos;

                int hit = detail_->view_cube_renderer_->hitTest(
                    detail_->viewport_, xpos, gl_y, cube_x, cube_y, size);

                if (hit >= 0) {
                    // Align camera to face the clicked element
                    switch (hit) {
                    case 0: detail_->viewport_.alignToAxis('x', true); break;  // +X
                    case 1: detail_->viewport_.alignToAxis('x', false); break; // -X
                    case 2: detail_->viewport_.alignToAxis('y', true); break;  // +Y
                    case 3: detail_->viewport_.alignToAxis('y', false); break; // -Y
                    case 4: detail_->viewport_.alignToAxis('z', true); break;  // +Z
                    case 5: detail_->viewport_.alignToAxis('z', false); break; // -Z
                    }

                    return; // Don't process normal camera controls
                }
            }

            // Initialize mouse position for camera controls
            detail_->viewport_.initScreenPos(glm::vec2(xpos, ypos));
        } else if (action == GLFW_RELEASE) {
            // Reset mouse initialization state when button is released
            detail_->viewport_.mouseInitialized = false;
        }
    }

    void ViewerDetail::cursorPosCallback(GLFWwindow* window, double x, double y) {
        // Check GUI
        if (detail_->gui_manager_ && detail_->gui_manager_->isAnyWindowActive()) {
            return;
        }

        // Update current mouse position
        glm::vec2 currentPos(x, y);

        // Orbit controls: Left mouse = rotate, Right mouse = pan
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            detail_->viewport_.rotate(currentPos);
        } else if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
            detail_->viewport_.translate(currentPos);
        }
    }

    void ViewerDetail::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
        // Check GUI
        if (detail_->gui_manager_ && detail_->gui_manager_->isAnyWindowActive()) {
            return;
        }

        float delta = static_cast<float>(yoffset);
        if (std::abs(delta) < 1.0e-2f)
            return;

        detail_->viewport_.zoom(delta);
    }

    void ViewerDetail::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        // Check GUI
        if (detail_->gui_manager_ && detail_->gui_manager_->isAnyWindowActive()) {
            return;
        }

        if (action == GLFW_PRESS || action == GLFW_REPEAT) {
            // Handle dataset navigation if available
            if (auto* gs_viewer = dynamic_cast<GSViewer*>(detail_)) {
                if (gs_viewer->dataset_panel_) {
                    bool handled = false;

                    switch (key) {
                    case GLFW_KEY_LEFT:
                        if (gs_viewer->dataset_panel_->getCurrentCameraIndex() > 0) {
                            int new_idx = gs_viewer->dataset_panel_->getCurrentCameraIndex() - 1;
                            // This is a bit of a hack - we need a setter in the panel
                            // For now, we'll handle it through the key callback
                            handled = true;
                        }
                        break;
                    case GLFW_KEY_RIGHT:
                        // Similar for right arrow
                        handled = true;
                        break;
                    case GLFW_KEY_ESCAPE:
                        if (gs_viewer->dataset_panel_->shouldShowImageOverlay()) {
                            // Toggle overlay off
                            handled = true;
                        }
                        break;
                    }

                    if (handled) return;
                }
            }

            // Original key handling
            switch (key) {
            case GLFW_KEY_G:
                detail_->show_grid_ = !detail_->show_grid_;
                break;
            case GLFW_KEY_F:
                // Focus on world origin
                detail_->viewport_.reset();
                break;
            case GLFW_KEY_H: // H for "home" view - look down at world origin
                detail_->viewport_.target = glm::vec3(0.0f, 0.0f, 0.0f);
                detail_->viewport_.azimuth = -135.0f;
                detail_->viewport_.elevation = -60.0f;
                detail_->viewport_.distance = 10.0f;
                std::cout << "Camera set to home view at world origin" << std::endl;
                break;
            }
        }
    }

    void ViewerDetail::run() {
        if (!init()) {
            std::cerr << "Viewer initialization failed!" << std::endl;
            return;
        }

        std::string shader_path = std::string(PROJECT_ROOT_PATH) + "/include/visualizer/shaders/";

        // Initialize shader manager
        try {
            shader_manager_ = std::make_unique<ShaderManager>(shader_path);
        } catch (const std::exception& e) {
            std::cerr << "Failed to initialize shader manager: " << e.what() << std::endl;
            return;
        }

        // Initialize GUI manager
        gui_manager_ = std::make_unique<GUIManager>();
        if (!gui_manager_->init(window_)) {
            std::cerr << "Failed to initialize GUI manager" << std::endl;
            return;
        }

        // Initialize grid renderer
        grid_renderer_ = std::make_unique<InfiniteGridRenderer>();
        if (!grid_renderer_->init(shader_path)) {
            std::cerr << "Failed to initialize infinite grid renderer" << std::endl;
            grid_renderer_.reset();
        } else {
            std::cout << "Grid renderer initialized successfully" << std::endl;
            show_grid_ = true;
            grid_plane_ = InfiniteGridRenderer::GridPlane::XZ;
        }

        // Load screen quad shader through shader manager
        try {
            shader_manager_->loadShader("screen_quad", "screen_quad.vert", "screen_quad.frag", true);
        } catch (const std::exception& e) {
            std::cerr << "Failed to load screen quad shader: " << e.what() << std::endl;
            return;
        }

        // Initialize screen renderer with interop support if available
#ifdef CUDA_GL_INTEROP_ENABLED
        screen_renderer_ = std::make_shared<ScreenQuadRendererInterop>(true);
        std::cout << "CUDA-OpenGL interop enabled for rendering" << std::endl;
#else
        screen_renderer_ = std::make_shared<ScreenQuadRenderer>();
        std::cout << "Using CPU copy for rendering (interop not available)" << std::endl;
#endif

        // Initialize view cube renderer
        std::cout << "Initializing view cube renderer..." << std::endl;
        view_cube_renderer_ = std::make_unique<ViewCubeRenderer>();
        if (!view_cube_renderer_->init(shader_path)) {
            std::cerr << "Failed to initialize view cube renderer" << std::endl;
            view_cube_renderer_.reset();
        } else {
            std::cout << "View cube renderer initialized successfully" << std::endl;
        }

        // Initialize camera frustum renderer
        std::cout << "Initializing camera frustum renderer..." << std::endl;
        try {
            camera_renderer_ = std::make_unique<CameraFrustumRenderer>();
            if (!camera_renderer_->init(shader_path)) {
                std::cerr << "Failed to initialize camera frustum renderer" << std::endl;
                camera_renderer_.reset();
            } else {
                std::cout << "Camera frustum renderer initialized successfully" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Exception during camera frustum renderer initialization: " << e.what() << std::endl;
            camera_renderer_.reset();
        }

        // NOW setup GUI panels if this is a GSViewer - after GUI manager is initialized
        std::cout << "Setting up GUI panels..." << std::endl;
        if (auto* gs_viewer = dynamic_cast<GSViewer*>(this)) {
            if (gs_viewer->hasTrainer() || gs_viewer->hasDataset()) {
                std::cout << "GSViewer has trainer or dataset, setting up panels..." << std::endl;
                gs_viewer->setupGUIPanels();
            }
        }

        while (!glfwWindowShouldClose(window_)) {
            controlFrameRate();
            updateWindowSize();

            // Update viewport for smooth camera transitions
            viewport_.update();

            draw();

            glfwSwapBuffers(window_);
            glfwPollEvents();
        }

        // Cleanup
        gui_manager_->shutdown();
        glfwDestroyWindow(window_);
        glfwTerminate();
    }

    // ============================================================================
    // GSViewer Implementation
    // ============================================================================

    GSViewer::GSViewer(std::string title, int width, int height)
        : ViewerDetail(title, width, height),
          trainer_(nullptr),
          dataset_(nullptr) {

        config_ = std::make_shared<RenderingConfig>();
        info_ = std::make_shared<TrainingInfo>();
        notifier_ = std::make_shared<Notifier>();

        setFrameRate(30);

        // Ensure grid is visible by default
        show_grid_ = true;
        grid_plane_ = InfiniteGridRenderer::GridPlane::XZ;

        std::cout << "GSViewer constructed with grid enabled" << std::endl;
    }

    GSViewer::~GSViewer() {
        // If trainer is still running, request it to stop
        if (trainer_ && trainer_->is_running()) {
            std::cout << "Viewer closing - stopping training..." << std::endl;
            trainer_->request_stop();

            // Give the training thread a moment to acknowledge the stop request
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::cout << "GSViewer destroyed." << std::endl;
    }

    void GSViewer::setTrainer(Trainer* trainer) {
        trainer_ = trainer;

        // Try to setup panels if GUI manager exists
        if (gui_manager_) {
            setupGUIPanels();
        }
        // Otherwise, panels will be set up later in run() or draw()
    }

    void GSViewer::setDataset(std::shared_ptr<CameraDataset> dataset) {
        dataset_ = dataset;

        // Try to setup panels if GUI manager exists
        if (gui_manager_) {
            setupGUIPanels();
        }
    }

    void GSViewer::setupGUIPanels() {
        std::cout << "GSViewer::setupGUIPanels() called" << std::endl;

        // Clear existing panels
        while (gui_manager_->getPanelCount() > 0) {
            gui_manager_->removePanel("Training Control");
            gui_manager_->removePanel("Rendering Settings");
            gui_manager_->removePanel("Camera Controls");
            gui_manager_->removePanel("Visualization Settings");
            gui_manager_->removePanel("Dataset Viewer");
        }

        try {
            // Create panels based on what's available
            if (trainer_) {
                std::cout << "Creating training panels..." << std::endl;
                auto training_panel = std::make_shared<TrainingControlPanel>(trainer_, info_);
                auto render_panel = std::make_shared<RenderSettingsPanel>(config_);
                gui_manager_->addPanel(training_panel);
                gui_manager_->addPanel(render_panel);
            }

            // Always add camera and visualization panels
            std::cout << "Creating camera and visualization panels..." << std::endl;
            auto camera_panel = std::make_shared<CameraControlPanel>(&viewport_);
            auto viz_panel = std::make_shared<VisualizationPanel>(
                grid_renderer_.get(),
                view_cube_renderer_.get(),
                &show_grid_,
                &show_view_cube_);

            gui_manager_->addPanel(camera_panel);
            gui_manager_->addPanel(viz_panel);

            // Add dataset viewer if dataset is available
            if (dataset_ && camera_renderer_) {
                std::cout << "Creating dataset viewer panel..." << std::endl;
                std::cout << "Dataset has " << dataset_->get_cameras().size() << " cameras" << std::endl;
                dataset_panel_ = std::make_shared<DatasetViewerPanel>(
                    dataset_, camera_renderer_.get(), &viewport_);
                gui_manager_->addPanel(dataset_panel_);
                std::cout << "Dataset viewer panel created successfully" << std::endl;
            }

            std::cout << "GUI panels setup complete" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Exception during GUI panel setup: " << e.what() << std::endl;
        }
    }

    void GSViewer::drawFrame() {
        // Only render if trainer is available
        if (!trainer_) {
            return;
        }

        // Update scene bounds if needed
        static bool bounds_initialized = false;
        if (!bounds_initialized && trainer_->get_strategy().get_model().size() > 0) {
            // Get approximate scene bounds from splat data
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

                // Make sure radius is reasonable
                if (scene_radius_ < 0.1f)
                    scene_radius_ = 1.0f;
                if (scene_radius_ > 100.0f)
                    scene_radius_ = 100.0f;

                scene_bounds_valid_ = true;

                std::cout << "Scene bounds - Center: (" << scene_center_.x << ", " << scene_center_.y << ", " << scene_center_.z
                          << "), Radius: " << scene_radius_ << std::endl;

                // Update camera's scene radius for proper speed scaling
                viewport_.camera.sceneRadius = scene_radius_;

                // Update zoom limits based on scene size
                viewport_.camera.minZoom = scene_radius_ * 0.01f;
                viewport_.camera.maxZoom = scene_radius_ * 100.0f;

                // Update camera panel with scene bounds
                auto camera_panel = std::dynamic_pointer_cast<CameraControlPanel>(
                    gui_manager_->getPanel("Camera Controls") // Note: use "Camera Controls" not "Camera Control"
                );
                if (camera_panel) {
                    camera_panel->setSceneBounds(scene_center_, scene_radius_);
                }

                bounds_initialized = true;

                // Don't auto-focus! Let user control camera
                std::cout << "Camera remains at world origin. Use mouse to navigate." << std::endl;
            }
        }

        // Get the OpenGL view matrix
        glm::mat4 view_opengl = viewport_.getViewMatrix();

        // The OpenGL view matrix transforms from world to camera space
        glm::mat4 view_inv = glm::inverse(view_opengl);
        glm::vec3 cam_pos_world = glm::vec3(view_inv[3]);

        // Convert from OpenGL to COLMAP coordinate system
        glm::mat4 opengl_to_colmap = glm::mat4(1.0f);
        opengl_to_colmap[1][1] = -1.0f; // Flip Y
        opengl_to_colmap[2][2] = -1.0f; // Flip Z

        // Apply the coordinate system transformation
        glm::mat4 view_colmap = opengl_to_colmap * view_opengl;

        // Extract the rotation and translation parts
        glm::mat3 R_w2c = glm::mat3(view_colmap);
        glm::vec3 t_w2c = glm::vec3(view_colmap[3]);

        // Create viewmat tensor
        torch::Tensor viewmat_tensor = torch::zeros({4, 4}, torch::kFloat32);

        // Fill in the rotation part
        viewmat_tensor[0][0] = R_w2c[0][0];
        viewmat_tensor[0][1] = R_w2c[0][1];
        viewmat_tensor[0][2] = R_w2c[0][2];
        viewmat_tensor[1][0] = R_w2c[1][0];
        viewmat_tensor[1][1] = R_w2c[1][1];
        viewmat_tensor[1][2] = R_w2c[1][2];
        viewmat_tensor[2][0] = R_w2c[2][0];
        viewmat_tensor[2][1] = R_w2c[2][1];
        viewmat_tensor[2][2] = R_w2c[2][2];

        // Fill in the translation part
        viewmat_tensor[0][3] = t_w2c[0];
        viewmat_tensor[1][3] = t_w2c[1];
        viewmat_tensor[2][3] = t_w2c[2];

        // Bottom row
        viewmat_tensor[3][0] = 0.0f;
        viewmat_tensor[3][1] = 0.0f;
        viewmat_tensor[3][2] = 0.0f;
        viewmat_tensor[3][3] = 1.0f;

        // Extract R and t for the Camera constructor
        torch::Tensor R_tensor = torch::tensor({R_w2c[0][0], R_w2c[1][0], R_w2c[2][0],
                                                R_w2c[0][1], R_w2c[1][1], R_w2c[2][1],
                                                R_w2c[0][2], R_w2c[1][2], R_w2c[2][2]},
                                               torch::TensorOptions().dtype(torch::kFloat32))
                                     .reshape({3, 3});

        torch::Tensor t_tensor = torch::tensor({t_w2c[0], t_w2c[1], t_w2c[2]},
                                               torch::TensorOptions().dtype(torch::kFloat32));

        glm::ivec2& reso = viewport_.windowSize;
        glm::vec2 fov = config_->getFov(reso.x, reso.y);

        // Create camera with world-to-camera transformation
        Camera cam = Camera(
            R_tensor,
            t_tensor,
            fov.x,
            fov.y,
            "viewer",
            "none",
            reso.x,
            reso.y,
            -1);

        // Use a transparent background so the grid shows through
        torch::Tensor background = torch::zeros({3});

        RenderOutput output;
        {
            std::lock_guard<std::mutex> lock(splat_mtx_);

            output = gs::rasterize(
                cam,
                trainer_->get_strategy().get_model(),
                background,
                config_->scaling_modifier,
                false,
                false,
                RenderMode::RGB);
        }

        // Clear depth buffer before rendering splats to ensure proper layering
        glClear(GL_DEPTH_BUFFER_BIT);

        // Enable blending to composite splats over the grid
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Disable depth testing for the screen quad
        glDisable(GL_DEPTH_TEST);

#ifdef CUDA_GL_INTEROP_ENABLED
        // Use interop for direct GPU transfer
        auto interop_renderer = std::dynamic_pointer_cast<ScreenQuadRendererInterop>(screen_renderer_);

        if (interop_renderer && interop_renderer->isInteropEnabled()) {
            // Keep data on GPU - convert [C, H, W] to [H, W, C] format
            auto image_hwc = output.image.permute({1, 2, 0}).contiguous();

            // We need to add alpha channel for proper blending
            torch::Tensor alpha = output.alpha;
            if (alpha.defined() && alpha.numel() > 0) {
                // Use the alpha from rasterization
                alpha = alpha.squeeze(0);
                if (alpha.dim() == 3 && alpha.size(0) == 1) {
                    alpha = alpha.squeeze(0);
                }
                // Combine RGB with alpha
                torch::Tensor rgba = torch::cat({image_hwc, alpha.unsqueeze(-1)}, -1);
                interop_renderer->uploadFromCUDA(rgba, reso.x, reso.y);
            } else {
                // If no alpha, just use RGB
                interop_renderer->uploadFromCUDA(image_hwc, reso.x, reso.y);
            }
        } else {
            // Fallback to CPU copy
            auto image = (output.image * 255).to(torch::kCPU).to(torch::kU8).permute({1, 2, 0}).contiguous();
            screen_renderer_->uploadData(image.data_ptr<uchar>(), reso.x, reso.y);
        }
#else
        // Original CPU copy path
        auto image = (output.image * 255).to(torch::kCPU).to(torch::kU8).permute({1, 2, 0}).contiguous();
        screen_renderer_->uploadData(image.data_ptr<uchar>(), reso.x, reso.y);
#endif

        // Get the shader from shader manager
        auto quadShader = shader_manager_->getShader("screen_quad");
        screen_renderer_->render(quadShader, viewport_);

        // Re-enable depth test for next frame
        glEnable(GL_DEPTH_TEST);
    }

    void GSViewer::drawGrid() {
        if (grid_renderer_ && show_grid_) {
            // Dynamically adjust fade distance based on scene size
            if (scene_bounds_valid_) {
                // Make grid visible up to 10x the scene radius
                float fade_start = scene_radius_ * 5.0f;
                float fade_end = scene_radius_ * 20.0f;

                // But ensure minimum visibility
                fade_start = std::max(fade_start, 1000.0f);
                fade_end = std::max(fade_end, 5000.0f);

                grid_renderer_->setFadeDistance(fade_start, fade_end);
            } else {
                // Default for when scene bounds aren't known yet
                grid_renderer_->setFadeDistance(1000.0f, 5000.0f);
            }

            grid_renderer_->render(viewport_, grid_plane_);
        }
    }

    void GSViewer::drawViewCube() {
        if (view_cube_renderer_ && show_view_cube_) {
            // Position in top-right corner
            float margin = 20.0f;
            float size = 120.0f;
            float x = viewport_.windowSize.x - margin - size / 2;
            float y = viewport_.windowSize.y - margin - size / 2;

            // View cube also uses the same viewport camera state
            view_cube_renderer_->render(viewport_, x, y, size);
        }
    }

    void GSViewer::drawCameras() {
        if (camera_renderer_ && dataset_panel_) {
            int highlight_idx = dataset_panel_->getCurrentCameraIndex();
            camera_renderer_->render(viewport_, highlight_idx);
        }
    }

    void GSViewer::drawImageOverlay() {
        if (!dataset_panel_ || !dataset_panel_->shouldShowImageOverlay()) {
            return;
        }

        auto image = dataset_panel_->getCurrentImage();
        if (!image.defined() || image.numel() == 0) {
            return;
        }

        // Simple overlay rendering in corner
        // This is a simplified version - you might want to use screen_renderer_
        // or create a dedicated overlay renderer
        glDisable(GL_DEPTH_TEST);

        // Draw semi-transparent overlay
        float overlay_width = 400.0f;
        float overlay_height = overlay_width * image.size(1) / image.size(2);
        float margin = 20.0f;

        // Position in bottom-right corner
        float x = viewport_.windowSize.x - overlay_width - margin;
        float y = margin;

        // Here you would render the image using a quad
        // For now, this is a placeholder

        glEnable(GL_DEPTH_TEST);
    }

    void GSViewer::draw() {
        // Check if we need to set up panels (deferred from setTrainer/setDataset)
        if ((trainer_ || dataset_) && gui_manager_ && gui_manager_->getPanelCount() == 0) {
            setupGUIPanels();
        }

        // Clear with a dark background
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 1. Draw grid first with depth testing
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        drawGrid();

        // 2. Draw camera frustums
        drawCameras();

        // 3. Draw splats if trainer is available
        if (trainer_) {
            // Clear depth buffer so splats render on top of grid
            glClear(GL_DEPTH_BUFFER_BIT);
            glDisable(GL_DEPTH_TEST);
            drawFrame();
            glEnable(GL_DEPTH_TEST);
        }

        // 4. Draw view cube (renders on top of everything)
        glDisable(GL_DEPTH_TEST);
        drawViewCube();
        glEnable(GL_DEPTH_TEST);

        // 5. Draw image overlay if enabled
        drawImageOverlay();

        // 6. GUI rendering
        gui_manager_->beginFrame();
        gui_manager_->render();

        // Handle training start trigger
        if (trainer_) {
            auto training_panel = std::dynamic_pointer_cast<TrainingControlPanel>(
                gui_manager_->getPanel("Training Control"));
            if (training_panel && training_panel->shouldStartTraining() && notifier_) {
                std::lock_guard<std::mutex> lock(notifier_->mtx);
                notifier_->ready = true;
                notifier_->cv.notify_one();
                training_panel->resetStartTrigger();
            }
        }

        gui_manager_->endFrame();
    }

} // namespace gs