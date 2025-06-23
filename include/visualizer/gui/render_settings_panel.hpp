#pragma once

#include "visualizer/gui/gui_manager.hpp"
#include <glm/glm.hpp>
#include <mutex>

namespace gs {

    class RenderSettingsPanel : public GUIPanel {
    public:
        struct RenderingConfig {
            std::mutex mtx;
            float fov = 60.0f;
            float scaling_modifier = 1.0f;

            glm::vec2 getFov(size_t reso_x, size_t reso_y) const {
                return glm::vec2(
                    atan(tan(glm::radians(fov) / 2.0f) * reso_x / reso_y) * 2.0f,
                    glm::radians(fov));
            }
        };

        RenderSettingsPanel(std::shared_ptr<RenderingConfig> config);

        void render() override;

    private:
        std::shared_ptr<RenderingConfig> config_;

        void renderFOVControl();
        void renderScaleControl();
        void renderRenderModeInfo();
    };

} // namespace gs