#include "visualizer/scene_renderer.hpp"
#include "config.h"
#include <iostream>

#ifdef CUDA_GL_INTEROP_ENABLED
#include "visualizer/cuda_gl_interop.hpp"
#endif

namespace gs {

    SceneRenderer::SceneRenderer() {
    }

    SceneRenderer::~SceneRenderer() {
    }

    bool SceneRenderer::initialize(const std::string& shader_path) {
        if (initialized_) {
            return true;
        }

        // Initialize shader manager
        try {
            shader_manager_ = std::make_unique<ShaderManager>(shader_path);
        } catch (const std::exception& e) {
            std::cerr << "Failed to initialize shader manager: " << e.what() << std::endl;
            return false;
        }

        // Initialize grid renderer
        grid_renderer_ = std::make_unique<InfiniteGridRenderer>();
        if (!grid_renderer_->init(shader_path)) {
            std::cerr << "Failed to initialize infinite grid renderer" << std::endl;
            grid_renderer_.reset();
        } else {
            std::cout << "Grid renderer initialized successfully" << std::endl;
        }

        // Load screen quad shader through shader manager
        try {
            shader_manager_->loadShader("screen_quad", "screen_quad.vert", "screen_quad.frag", true);
        } catch (const std::exception& e) {
            std::cerr << "Failed to load screen quad shader: " << e.what() << std::endl;
            return false;
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

        initialized_ = true;
        return true;
    }

    void SceneRenderer::updateSceneBounds(const glm::vec3& center, float radius) {
        scene_center_ = center;
        scene_radius_ = radius;
        scene_bounds_valid_ = true;
    }

    void SceneRenderer::setCameras(const std::vector<std::shared_ptr<Camera>>& cameras,
                                   const std::vector<bool>& is_test_camera) {
        if (camera_renderer_) {
            camera_renderer_->setCameras(cameras, is_test_camera);
        }
    }

    void SceneRenderer::renderGrid(const Viewport& viewport, const RenderSettings& settings) {
        if (!grid_renderer_ || !settings.show_grid) {
            return;
        }

        // Dynamically adjust fade distance based on scene size
        if (scene_bounds_valid_) {
            float fade_start = scene_radius_ * 5.0f;
            float fade_end = scene_radius_ * 20.0f;

            // Ensure minimum visibility
            fade_start = std::max(fade_start, 1000.0f);
            fade_end = std::max(fade_end, 5000.0f);

            grid_renderer_->setFadeDistance(fade_start, fade_end);
        } else {
            grid_renderer_->setFadeDistance(1000.0f, 5000.0f);
        }

        grid_renderer_->render(viewport, settings.grid_plane);
    }

    void SceneRenderer::renderViewCube(const Viewport& viewport, bool show) {
        if (!view_cube_renderer_ || !show) {
            return;
        }

        // Position in top-right corner
        float x = viewport.windowSize.x - view_cube_margin_ - view_cube_size_ / 2;
        float y = viewport.windowSize.y - view_cube_margin_ - view_cube_size_ / 2;

        view_cube_renderer_->render(viewport, x, y, view_cube_size_);
    }

    void SceneRenderer::renderCameras(const Viewport& viewport, int highlight_index) {
        if (!camera_renderer_) {
            return;
        }

        camera_renderer_->render(viewport, highlight_index);
    }

    void SceneRenderer::renderSplats(const Viewport& viewport,
                                     Trainer* trainer,
                                     std::shared_ptr<RenderSettingsPanel::RenderingConfig> config,
                                     std::mutex& splat_mutex) {
        if (!trainer || !initialized_) {
            return;
        }

        // Convert viewport to COLMAP camera
        glm::mat4 view_opengl = viewport.getViewMatrix();
        glm::mat4 view_inv = glm::inverse(view_opengl);

        // Convert from OpenGL to COLMAP coordinate system
        glm::mat4 opengl_to_colmap = glm::mat4(1.0f);
        opengl_to_colmap[1][1] = -1.0f; // Flip Y
        opengl_to_colmap[2][2] = -1.0f; // Flip Z

        glm::mat4 view_colmap = opengl_to_colmap * view_opengl;
        glm::mat3 R_w2c = glm::mat3(view_colmap);
        glm::vec3 t_w2c = glm::vec3(view_colmap[3]);

        // Create tensors for Camera
        torch::Tensor R_tensor = torch::zeros({3, 3}, torch::kFloat32);
        torch::Tensor t_tensor = torch::zeros({3}, torch::kFloat32);

        // Fill R_tensor
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                R_tensor[i][j] = R_w2c[j][i]; // Transpose for row-major
            }
        }

        // Fill t_tensor
        t_tensor[0] = t_w2c[0];
        t_tensor[1] = t_w2c[1];
        t_tensor[2] = t_w2c[2];

        glm::ivec2 reso = viewport.windowSize;
        glm::vec2 fov = config->getFov(reso.x, reso.y);

        // Create camera
        Camera cam(R_tensor, t_tensor, fov.x, fov.y, "viewer", "none", reso.x, reso.y, -1);

        // Render with transparent background
        torch::Tensor background = torch::zeros({3});

        RenderOutput output;
        {
            std::lock_guard<std::mutex> lock(splat_mutex);
            output = gs::rasterize(
                cam,
                trainer->get_strategy().get_model(),
                background,
                config->scaling_modifier,
                false,
                false,
                RenderMode::RGB);
        }

        // Upload to screen renderer
        OpenGLStateManager::StateGuard guard(getGLStateManager());
        glClear(GL_DEPTH_BUFFER_BIT);
        getGLStateManager().setForSplatRendering();

#ifdef CUDA_GL_INTEROP_ENABLED
        auto interop_renderer = std::dynamic_pointer_cast<ScreenQuadRendererInterop>(screen_renderer_);
        if (interop_renderer && interop_renderer->isInteropEnabled()) {
            auto image_hwc = output.image.permute({1, 2, 0}).contiguous();

            torch::Tensor alpha = output.alpha;
            if (alpha.defined() && alpha.numel() > 0) {
                alpha = alpha.squeeze(0);
                if (alpha.dim() == 3 && alpha.size(0) == 1) {
                    alpha = alpha.squeeze(0);
                }
                torch::Tensor rgba = torch::cat({image_hwc, alpha.unsqueeze(-1)}, -1);
                interop_renderer->uploadFromCUDA(rgba, reso.x, reso.y);
            } else {
                interop_renderer->uploadFromCUDA(image_hwc, reso.x, reso.y);
            }
        } else {
            auto image = (output.image * 255).to(torch::kCPU).to(torch::kU8).permute({1, 2, 0}).contiguous();
            screen_renderer_->uploadData(image.data_ptr<unsigned char>(), reso.x, reso.y);
        }
#else
        auto image = (output.image * 255).to(torch::kCPU).to(torch::kU8).permute({1, 2, 0}).contiguous();
        screen_renderer_->uploadData(image.data_ptr<unsigned char>(), reso.x, reso.y);
#endif

        auto quadShader = shader_manager_->getShader("screen_quad");
        screen_renderer_->render(quadShader, viewport);
    }

    void SceneRenderer::renderImageOverlay(const Viewport& viewport,
                                           const torch::Tensor& image,
                                           float x, float y, float width, float height) {
        if (!image.defined() || image.numel() == 0) {
            return;
        }

        OpenGLStateManager::StateGuard guard(getGLStateManager());
        glDisable(GL_DEPTH_TEST);

        // TODO: Implement actual image overlay rendering
        // This would require creating a quad renderer for overlays
        // For now this is a placeholder
    }

    int SceneRenderer::hitTestViewCube(const Viewport& viewport, float screen_x, float screen_y) {
        if (!view_cube_renderer_) {
            return -1;
        }

        float x = viewport.windowSize.x - view_cube_margin_ - view_cube_size_ / 2;
        float y = viewport.windowSize.y - view_cube_margin_ - view_cube_size_ / 2;

        // Convert to OpenGL coordinates (flip Y)
        float gl_y = viewport.windowSize.y - screen_y;

        return view_cube_renderer_->hitTest(viewport, screen_x, gl_y, x, y, view_cube_size_);
    }

} // namespace gs