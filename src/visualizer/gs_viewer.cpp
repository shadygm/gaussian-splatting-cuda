#include "visualizer/gs_viewer.hpp"
#include <iostream>
#include <thread>

namespace gs {

    GSViewer::GSViewer(const std::string& title, int width, int height)
        : ViewerBase(title, width, height) {

        render_config_ = std::make_shared<RenderingConfig>();
        training_info_ = std::make_shared<TrainingInfo>();
        notifier_ = std::make_shared<Notifier>();

        setTargetFPS(30);

        // Default render settings
        render_settings_.show_grid = true;
        render_settings_.show_view_cube = true;
        render_settings_.grid_plane = InfiniteGridRenderer::GridPlane::XZ;

        std::cout << "GSViewer constructed" << std::endl;
    }

    GSViewer::~GSViewer() {
        // If trainer is still running, request it to stop
        if (trainer_ && trainer_->is_running()) {
            std::cout << "Viewer closing - stopping training..." << std::endl;
            trainer_->request_stop();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::cout << "GSViewer destroyed." << std::endl;
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