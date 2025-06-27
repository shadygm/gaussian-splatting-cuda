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

        // Initialize rotation gizmo
        std::cout << "Initializing rotation gizmo..." << std::endl;
        rotation_gizmo_ = std::make_unique<RotationGizmo>();
        if (!rotation_gizmo_->init(shader_path)) {
            std::cerr << "Failed to initialize rotation gizmo" << std::endl;
            rotation_gizmo_.reset();
        } else {
            std::cout << "Rotation gizmo initialized successfully" << std::endl;
        }

        // Initialize translation gizmo
        std::cout << "Initializing translation gizmo..." << std::endl;
        translation_gizmo_ = std::make_unique<TranslationGizmo>();
        if (!translation_gizmo_->init(shader_path)) {
            std::cerr << "Failed to initialize translation gizmo" << std::endl;
            translation_gizmo_.reset();
        } else {
            std::cout << "Translation gizmo initialized successfully" << std::endl;
        }

        initialized_ = true;
        return true;
    }

    void SceneRenderer::updateSceneBounds(const glm::vec3& center, float radius) {
        scene_center_ = center;
        scene_radius_ = radius;
        scene_bounds_valid_ = true;

        // Update gizmo positions to match scene center
        updateGizmoPosition(center);
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

        // Get the combined scene transform from both gizmos
        glm::mat4 scene_transform = getSceneTransform();

        // Debug output - print transform whenever it changes
        static glm::mat4 last_transform = glm::mat4(1.0f);
        if (scene_transform != last_transform) {
            std::cout << "Scene transform changed:\n";
            for (int i = 0; i < 4; ++i) {
                std::cout << scene_transform[i][0] << " " << scene_transform[i][1]
                          << " " << scene_transform[i][2] << " " << scene_transform[i][3] << "\n";
            }
            last_transform = scene_transform;
        }

        // Pass transform to camera renderer
        camera_renderer_->setSceneTransform(scene_transform);
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

        // Apply combined scene transform (inverse because we're transforming the view)
        glm::mat4 scene_transform = getSceneTransform();
        view_colmap = view_colmap * glm::inverse(scene_transform);

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

            // Instead of modifying the model directly, we'll transform the camera
            // The effect is the same - rotating the scene is equivalent to
            // rotating the camera in the opposite direction
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

    void SceneRenderer::renderGizmo(const Viewport& viewport) {
        if (gizmo_mode_ == GizmoMode::ROTATION && rotation_gizmo_ && rotation_gizmo_->isVisible()) {
            // Update gizmo position and size based on scene
            if (scene_bounds_valid_) {
                // Rotation gizmo stays at the transformed scene center
                glm::vec3 gizmo_pos = scene_center_;
                if (translation_gizmo_) {
                    // Add translation offset
                    gizmo_pos += translation_gizmo_->getTranslation();
                }
                rotation_gizmo_->setPosition(gizmo_pos);

                // Calculate appropriate gizmo size based on viewport distance
                glm::vec3 cam_pos = viewport.getCameraPosition();
                float distance_to_center = glm::length(cam_pos - gizmo_pos);

                // Make gizmo size relative to view distance, not scene size
                float gizmo_radius = distance_to_center * 0.1f;      // 10% of view distance
                gizmo_radius = glm::clamp(gizmo_radius, 0.1f, 3.0f); // Keep it reasonable

                rotation_gizmo_->setRadius(gizmo_radius);
            }

            rotation_gizmo_->render(viewport);
        } else if (gizmo_mode_ == GizmoMode::TRANSLATION && translation_gizmo_ && translation_gizmo_->isVisible()) {
            // Update gizmo position and size based on scene
            if (scene_bounds_valid_) {
                // Translation gizmo follows the accumulated translation
                translation_gizmo_->setPosition(scene_center_);

                // Calculate appropriate gizmo size based on viewport distance
                glm::vec3 cam_pos = viewport.getCameraPosition();
                glm::vec3 gizmo_pos = translation_gizmo_->getPosition();
                float distance_to_gizmo = glm::length(cam_pos - gizmo_pos);

                // Make gizmo size relative to view distance
                float gizmo_scale = distance_to_gizmo * 0.1f;      // 10% of view distance
                gizmo_scale = glm::clamp(gizmo_scale, 0.1f, 3.0f); // Keep it reasonable

                translation_gizmo_->setScale(gizmo_scale);
            }

            translation_gizmo_->render(viewport);
        }
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

    void SceneRenderer::setGizmoMode(GizmoMode mode) {
        gizmo_mode_ = mode;

        // Hide all gizmos first
        if (rotation_gizmo_)
            rotation_gizmo_->setVisible(false);
        if (translation_gizmo_)
            translation_gizmo_->setVisible(false);

        // Show the selected gizmo
        switch (mode) {
        case GizmoMode::ROTATION:
            if (rotation_gizmo_)
                rotation_gizmo_->setVisible(true);
            break;
        case GizmoMode::TRANSLATION:
            if (translation_gizmo_)
                translation_gizmo_->setVisible(true);
            break;
        case GizmoMode::NONE:
        default:
            break;
        }
    }

    void SceneRenderer::setGizmoVisible(bool visible) {
        if (gizmo_mode_ == GizmoMode::ROTATION && rotation_gizmo_) {
            rotation_gizmo_->setVisible(visible);
        } else if (gizmo_mode_ == GizmoMode::TRANSLATION && translation_gizmo_) {
            translation_gizmo_->setVisible(visible);
        }
    }

    bool SceneRenderer::isGizmoVisible() const {
        if (gizmo_mode_ == GizmoMode::ROTATION) {
            return rotation_gizmo_ && rotation_gizmo_->isVisible();
        } else if (gizmo_mode_ == GizmoMode::TRANSLATION) {
            return translation_gizmo_ && translation_gizmo_->isVisible();
        }
        return false;
    }

    glm::mat4 SceneRenderer::getSceneTransform() const {
        glm::mat4 rotation_transform(1.0f);
        glm::mat4 translation_transform(1.0f);

        if (rotation_gizmo_) {
            rotation_transform = rotation_gizmo_->getTransformMatrix();
        }

        if (translation_gizmo_) {
            translation_transform = translation_gizmo_->getTransformMatrix();
        }

        // Combine transforms: first translate, then rotate
        return rotation_transform * translation_transform;
    }

    void SceneRenderer::updateGizmoPosition(const glm::vec3& position) {
        scene_center_ = position;
        if (rotation_gizmo_) {
            rotation_gizmo_->setPosition(position);
        }
        if (translation_gizmo_) {
            translation_gizmo_->setPosition(position);
        }
    }

} // namespace gs