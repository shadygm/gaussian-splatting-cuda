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

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        io.ConfigWindowsMoveFromTitleBarOnly = true;
        ImGui::StyleColorsLight();

        // Setup Platform/Renderer backends
        const char* glsl_version = "#version 430";
        ImGui_ImplGlfw_InitForOpenGL(window_, true);
        ImGui_ImplOpenGL3_Init(glsl_version);

        // Set Fonts
        std::string font_path = std::string(PROJECT_ROOT_PATH) +
                                "/include/visualizer/assets/JetBrainsMono-Regular.ttf";
        io.Fonts->AddFontFromFileTTF(font_path.c_str(), 14.0f);

        // Set Windows option
        window_flags |= ImGuiWindowFlags_NoScrollbar;
        window_flags |= ImGuiWindowFlags_NoResize;

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);

        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
        style.WindowPadding = ImVec2(6.0f, 6.0f);
        style.WindowRounding = 6.0f;
        style.WindowBorderSize = 0.0f;

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

    float ViewerDetail::getGPUUsage() {
        size_t free_byte, total_byte;
        cudaDeviceSynchronize();
        cudaMemGetInfo(&free_byte, &total_byte);
        size_t used_byte = total_byte - free_byte;
        float gpuUsage = used_byte / (float)total_byte * 100;

        return gpuUsage;
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
        // First check ImGui
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse) {
            return;
        }

        if (detail_->any_window_active)
            return;

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
        // Check ImGui
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse) {
            return;
        }

        if (detail_->any_window_active)
            return;

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
        // Check ImGui
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse) {
            return;
        }

        if (detail_->any_window_active)
            return;

        float delta = static_cast<float>(yoffset);
        if (std::abs(delta) < 1.0e-2f)
            return;

        detail_->viewport_.zoom(delta);
    }

    void ViewerDetail::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        // Check ImGui
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureKeyboard) {
            return;
        }

        if (action == GLFW_PRESS || action == GLFW_REPEAT) {
            switch (key) {
            case GLFW_KEY_G:
                if (detail_->show_grid_) {
                    detail_->show_grid_ = false;
                } else {
                    detail_->show_grid_ = true;
                }
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

        // Initialize grid renderer FIRST
        grid_renderer_ = std::make_unique<InfiniteGridRenderer>();
        if (!grid_renderer_->init(shader_path)) {
            std::cerr << "Failed to initialize infinite grid renderer" << std::endl;
            grid_renderer_.reset();
        } else {
            std::cout << "Grid renderer initialized successfully" << std::endl;
            // Set initial grid visibility to true
            show_grid_ = true;
            grid_plane_ = InfiniteGridRenderer::GridPlane::XZ; // Ensure XZ plane
        }

        // Then initialize other renderers...
        quadShader_ = std::make_shared<Shader>(
            (shader_path + "/screen_quad.vert").c_str(),
            (shader_path + "/screen_quad.frag").c_str(),
            true);

        // Initialize screen renderer with interop support if available
#ifdef CUDA_GL_INTEROP_ENABLED
        screen_renderer_ = std::make_shared<ScreenQuadRendererInterop>(true);
        std::cout << "CUDA-OpenGL interop enabled for rendering" << std::endl;
#else
        screen_renderer_ = std::make_shared<ScreenQuadRenderer>();
        std::cout << "Using CPU copy for rendering (interop not available)" << std::endl;
#endif

        // Initialize view cube renderer
        view_cube_renderer_ = std::make_unique<ViewCubeRenderer>();
        if (!view_cube_renderer_->init(shader_path)) {
            std::cerr << "Failed to initialize view cube renderer" << std::endl;
            view_cube_renderer_.reset();
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
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        glfwDestroyWindow(window_);
        glfwTerminate();
    }

    GSViewer::GSViewer(std::string title, int width, int height)
        : ViewerDetail(title, width, height),
          trainer_(nullptr) {

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
    }

    void GSViewer::drawFrame() {
        // Only render if trainer is available
        if (!trainer_) {
            return;
        }

        // Update scene bounds if needed (but don't auto-focus on them)
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

                bounds_initialized = true;

                // Don't auto-focus! Let user control camera
                std::cout << "Camera remains at world origin. Use mouse to navigate." << std::endl;
            }
        }

        // Get the OpenGL view matrix
        glm::mat4 view_opengl = viewport_.getViewMatrix();

        // The OpenGL view matrix transforms from world to camera space
        // Extract camera position in world space
        glm::mat4 view_inv = glm::inverse(view_opengl);
        glm::vec3 cam_pos_world = glm::vec3(view_inv[3]);

        // Now we need to build the viewmat in the format expected by the CUDA kernel
        // The kernel expects a row-major 4x4 matrix where:
        // - The upper-left 3x3 is the rotation matrix R (world-to-camera)
        // - The first 3 elements of the last column are the translation t
        // - The transformation is: p_camera = R * p_world + t

        // Since COLMAP uses a different coordinate system than OpenGL:
        // OpenGL: Y-up, camera looks down -Z
        // COLMAP: Y-down, camera looks down +Z

        // We need to apply a 180-degree rotation around X to convert from OpenGL to COLMAP
        glm::mat4 opengl_to_colmap = glm::mat4(1.0f);
        opengl_to_colmap[1][1] = -1.0f; // Flip Y
        opengl_to_colmap[2][2] = -1.0f; // Flip Z

        // Apply the coordinate system transformation
        glm::mat4 view_colmap = opengl_to_colmap * view_opengl;

        // Extract the rotation and translation parts
        glm::mat3 R_w2c = glm::mat3(view_colmap);
        glm::vec3 t_w2c = glm::vec3(view_colmap[3]);

        // Now create the viewmat in row-major format as expected by the kernel
        torch::Tensor viewmat_tensor = torch::zeros({4, 4}, torch::kFloat32);

        // Fill in the rotation part (remember, we're storing in row-major)
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

        // Extract R and t for the Camera constructor (which uses a different format)
        // The Camera constructor expects R and T such that world_to_view constructs the proper matrix
        // Looking at world_to_view in camera.cpp, it transposes R and creates a specific format

        // For now, let's just extract what we need
        torch::Tensor R_tensor = torch::tensor({R_w2c[0][0], R_w2c[1][0], R_w2c[2][0], // Note: transposed for row-major
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

        screen_renderer_->render(quadShader_, viewport_);

        // Re-enable depth test for next frame
        glEnable(GL_DEPTH_TEST);
    }

    void GSViewer::drawGrid() {
        static bool first_call = true;
        if (first_call) {
            std::cout << "First drawGrid call - grid_renderer_: " << (grid_renderer_ ? "valid" : "null")
                      << ", show_grid_: " << show_grid_ << std::endl;
            first_call = false;
        }

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

    void GSViewer::configuration() {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Update the any_window_active flag based on ImGui state
        any_window_active = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) ||
                            ImGui::IsAnyItemActive() ||
                            ImGui::IsAnyItemHovered();

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.5f, 0.5f, 0.5f, 0.8f));
        ImGui::Begin("Rendering Setting", nullptr, window_flags);
        ImGui::SetWindowSize(ImVec2(300, 0));

        // Check if trainer is set
        if (!trainer_) {
            ImGui::Text("Waiting for trainer initialization...");
            ImGui::End();
            ImGui::PopStyleColor();
            return;
        }

        // Training control section
        ImGui::Separator();
        ImGui::Text("Training Control");
        ImGui::Separator();

        bool is_training = trainer_->is_running();
        bool is_paused = trainer_->is_paused();
        bool is_complete = trainer_->is_training_complete();
        bool has_stopped = trainer_->has_stopped();

        // Show appropriate controls based on state
        if (!training_started_ && !is_training) {
            // Initial state - show start button
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
            if (ImGui::Button("Start Training", ImVec2(-1, 0))) {
                manual_start_triggered_ = true;
                training_started_ = true;
            }
            ImGui::PopStyleColor(2);
        } else if (is_complete || has_stopped) {
            // Training finished - show status
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f),
                               has_stopped ? "Training Stopped!" : "Training Complete!");
        } else {
            // Training in progress - show control buttons

            // Pause/Resume button
            if (is_paused) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
                if (ImGui::Button("Resume", ImVec2(-1, 0))) {
                    trainer_->request_resume();
                }
                ImGui::PopStyleColor(2);

                // When paused, show stop button too
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
                if (ImGui::Button("Stop Permanently", ImVec2(-1, 0))) {
                    trainer_->request_stop();
                }
                ImGui::PopStyleColor(2);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.5f, 0.1f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.6f, 0.2f, 1.0f));
                if (ImGui::Button("Pause", ImVec2(-1, 0))) {
                    trainer_->request_pause();
                }
                ImGui::PopStyleColor(2);
            }

            // Save checkpoint button (always visible during training)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.4f, 0.7f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
            if (ImGui::Button("Save Checkpoint", ImVec2(-1, 0))) {
                trainer_->request_save();
                save_in_progress_ = true;
                save_start_time_ = std::chrono::steady_clock::now();
            }
            ImGui::PopStyleColor(2);
        }

        // Show save progress feedback
        if (save_in_progress_) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - save_start_time_).count();
            if (elapsed < 2000) {
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Checkpoint saved!");
            } else {
                save_in_progress_ = false;
            }
        }

        // Status display
        ImGui::Separator();
        int current_iter = trainer_->get_current_iteration();
        float current_loss = trainer_->get_current_loss();
        ImGui::Text("Status: %s", is_complete ? "Complete" : (is_paused ? "Paused" : (is_training ? "Training" : "Ready")));
        ImGui::Text("Iteration: %d", current_iter);
        ImGui::Text("Loss: %.6f", current_loss);

        // Display render mode
#ifdef CUDA_GL_INTEROP_ENABLED
        ImGui::Text("Render Mode: GPU Direct (Interop)");
#else
        ImGui::Text("Render Mode: CPU Copy");
#endif

        // Handle the start trigger
        if (notifier_ && manual_start_triggered_) {
            std::lock_guard<std::mutex> lock(notifier_->mtx);
            notifier_->ready = true;
            notifier_->cv.notify_one();
            manual_start_triggered_ = false;
        }

        ImGui::Separator();
        ImGui::Text("Rendering Settings");
        ImGui::Separator();

        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("##scale_slider", &config_->scaling_modifier, 0.01f, 3.0f, "Scale=%.2f");
        ImGui::SameLine();
        if (ImGui::Button("Reset##scale", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
            config_->scaling_modifier = 1.0f;
        }

        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("##fov_slider", &config_->fov, 45.0f, 120.0f, "FoV=%.2f");
        ImGui::SameLine();
        if (ImGui::Button("Reset##fov", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
            config_->fov = 75.0f;
        }

        // Camera Info and Controls
        ImGui::Separator();
        ImGui::Text("Camera Controls");
        ImGui::Separator();

        ImGui::Text("Left Mouse: Orbit");
        ImGui::Text("Right Mouse: Pan");
        ImGui::Text("Scroll: Zoom");
        ImGui::Text("G: Toggle Grid");
        ImGui::Text("F: Focus World Origin");
        ImGui::Text("H: Home View (Look Down)");

        if (ImGui::Button("Reset Camera", ImVec2(-1, 0))) {
            viewport_.reset();
        }

        // Camera parameters
        ImGui::Text("Distance: %.2f", viewport_.distance);
        ImGui::Text("Azimuth: %.1f°", viewport_.azimuth);
        ImGui::Text("Elevation: %.1f°", viewport_.elevation);
        ImGui::Text("Target: %.2f, %.2f, %.2f",
                    viewport_.target.x,
                    viewport_.target.y,
                    viewport_.target.z);

        // Scene info (if available)
        if (scene_bounds_valid_) {
            ImGui::Separator();
            ImGui::Text("Scene Info:");
            ImGui::Text("Center: %.2f, %.2f, %.2f", scene_center_.x, scene_center_.y, scene_center_.z);
            ImGui::Text("Radius: %.2f", scene_radius_);
        }

        // Quick view buttons
        ImGui::Text("Quick Views:");
        if (ImGui::Button("Front", ImVec2(60, 0)))
            viewport_.alignToAxis('z', true);
        ImGui::SameLine();
        if (ImGui::Button("Back", ImVec2(60, 0)))
            viewport_.alignToAxis('z', false);
        ImGui::SameLine();
        if (ImGui::Button("Left", ImVec2(60, 0)))
            viewport_.alignToAxis('x', false);
        ImGui::SameLine();
        if (ImGui::Button("Right", ImVec2(60, 0)))
            viewport_.alignToAxis('x', true);

        if (ImGui::Button("Top", ImVec2(60, 0)))
            viewport_.alignToAxis('y', true);
        ImGui::SameLine();
        if (ImGui::Button("Bottom", ImVec2(60, 0)))
            viewport_.alignToAxis('y', false);

        // Grid Settings Section
        ImGui::Separator();
        ImGui::Text("Grid Settings");
        ImGui::Separator();

        ImGui::Checkbox("Show Grid", &show_grid_);

        if (show_grid_ && grid_renderer_) {
            static float grid_opacity = 1.0f;
            if (ImGui::SliderFloat("Grid Opacity", &grid_opacity, 0.0f, 1.0f)) {
                grid_renderer_->setOpacity(grid_opacity);
            }
        }

        // View Cube Settings Section
        ImGui::Separator();
        ImGui::Text("View Cube");
        ImGui::Separator();

        ImGui::Checkbox("Show View Cube", &show_view_cube_);

        int current_iter2;
        int total_iter;
        int num_splats;
        std::vector<float> loss_data;
        {
            std::lock_guard<std::mutex> lock(info_->mtx);
            current_iter2 = info_->curr_iterations_;
            total_iter = info_->total_iterations_;
            num_splats = info_->num_splats_;
            loss_data.assign(info_->loss_buffer_.begin(), info_->loss_buffer_.end());
        }

        float fraction = total_iter > 0 ? float(current_iter2) / float(total_iter) : 0.0f;
        char overlay_text[64];
        std::snprintf(overlay_text, sizeof(overlay_text), "%d / %d", current_iter2, total_iter);
        ImGui::ProgressBar(fraction, ImVec2(-1, 20), overlay_text);

        if (loss_data.size() > 0) {
            auto [min_it, max_it] = std::minmax_element(loss_data.begin(), loss_data.end());
            float min_val = *min_it, max_val = *max_it;

            if (min_val == max_val) {
                min_val -= 1.0f;
                max_val += 1.0f;
            } else {
                float margin = (max_val - min_val) * 0.05f;
                min_val -= margin;
                max_val += margin;
            }

            char loss_label[64];
            std::snprintf(loss_label, sizeof(loss_label), "Loss: %.4f", loss_data.back());

            ImGui::PlotLines(
                "##Loss",
                loss_data.data(),
                static_cast<int>(loss_data.size()),
                0,
                loss_label,
                min_val,
                max_val,
                ImVec2(-1, 50));
        }

        float gpuUsage = getGPUUsage();
        char gpuText[64];
        std::snprintf(gpuText, sizeof(gpuText), "GPU Usage: %.1f%%", gpuUsage);
        ImGui::ProgressBar(gpuUsage / 100.0f, ImVec2(-1, 20), gpuText);

        ImGui::Text("num Splats: %d", num_splats);

        ImGui::End();
        ImGui::PopStyleColor();
    }

    void GSViewer::draw() {
        // Clear with a dark background
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 1. Draw grid first with depth testing
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        drawGrid();

        // 2. Draw splats if trainer is available
        if (trainer_) {
            // Clear depth buffer so splats render on top of grid
            glClear(GL_DEPTH_BUFFER_BIT);
            glDisable(GL_DEPTH_TEST);
            drawFrame();
            glEnable(GL_DEPTH_TEST);
        }

        // 3. Draw view cube (renders on top of everything)
        glDisable(GL_DEPTH_TEST);
        drawViewCube();
        glEnable(GL_DEPTH_TEST);

        // 4. ImGui UI (renders on top of everything)
        configuration();

        // Render all ImGui elements
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

} // namespace gs