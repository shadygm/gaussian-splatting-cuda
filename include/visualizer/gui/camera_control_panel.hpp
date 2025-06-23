#pragma once

#include "visualizer/gui/gui_manager.hpp"
#include "visualizer/viewport.hpp"
#include <glm/glm.hpp>

namespace gs {

    class CameraControlPanel : public GUIPanel {
    public:
        CameraControlPanel(Viewport* viewport);

        void render() override;

        // Scene bounds for display
        void setSceneBounds(const glm::vec3& center, float radius);

    private:
        void renderCameraInfo();
        void renderCameraControls();
        void renderQuickViewButtons();
        void renderSceneInfo();

        Viewport* viewport_;

        // Scene info
        bool scene_bounds_valid_ = false;
        glm::vec3 scene_center_{0.0f, 0.0f, 0.0f};
        float scene_radius_ = 1.0f;
    };

} // namespace gs