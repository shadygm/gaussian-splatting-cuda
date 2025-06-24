#pragma once

#include "visualizer/gl_headers.hpp"
#include <stack>

namespace gs {

    class OpenGLStateManager {
    public:
        struct State {
            GLboolean depth_test;
            GLboolean blend;
            GLboolean cull_face;
            GLboolean scissor_test;
            GLint blend_src;
            GLint blend_dst;
            GLenum depth_func;
            GLenum cull_face_mode;
            GLenum front_face;
            GLfloat line_width;
            GLint viewport[4];
            GLboolean depth_mask;
            GLint polygon_mode[2];
        };

        // Save current OpenGL state
        State save() const {
            State state;
            state.depth_test = glIsEnabled(GL_DEPTH_TEST);
            state.blend = glIsEnabled(GL_BLEND);
            state.cull_face = glIsEnabled(GL_CULL_FACE);
            state.scissor_test = glIsEnabled(GL_SCISSOR_TEST);
            glGetIntegerv(GL_BLEND_SRC_ALPHA, &state.blend_src);
            glGetIntegerv(GL_BLEND_DST_ALPHA, &state.blend_dst);
            glGetIntegerv(GL_DEPTH_FUNC, (GLint*)&state.depth_func);
            glGetIntegerv(GL_CULL_FACE_MODE, (GLint*)&state.cull_face_mode);
            glGetIntegerv(GL_FRONT_FACE, (GLint*)&state.front_face);
            glGetFloatv(GL_LINE_WIDTH, &state.line_width);
            glGetIntegerv(GL_VIEWPORT, state.viewport);
            glGetBooleanv(GL_DEPTH_WRITEMASK, &state.depth_mask);
            glGetIntegerv(GL_POLYGON_MODE, state.polygon_mode);
            return state;
        }

        // Restore OpenGL state
        void restore(const State& state) const {
            setEnabled(GL_DEPTH_TEST, state.depth_test);
            setEnabled(GL_BLEND, state.blend);
            setEnabled(GL_CULL_FACE, state.cull_face);
            setEnabled(GL_SCISSOR_TEST, state.scissor_test);

            if (state.blend) {
                glBlendFunc(state.blend_src, state.blend_dst);
            }

            glDepthFunc(state.depth_func);
            glDepthMask(state.depth_mask);
            glCullFace(state.cull_face_mode);
            glFrontFace(state.front_face);
            glLineWidth(state.line_width);
            glViewport(state.viewport[0], state.viewport[1],
                       state.viewport[2], state.viewport[3]);
            glPolygonMode(GL_FRONT_AND_BACK, state.polygon_mode[0]);
        }

        // RAII helper for state management
        class StateGuard {
        public:
            StateGuard(const OpenGLStateManager& manager)
                : manager_(manager),
                  state_(manager.save()) {}

            ~StateGuard() {
                manager_.restore(state_);
            }

        private:
            const OpenGLStateManager& manager_;
            State state_;
        };

        // Common state presets
        void setForSplatRendering() {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_DEPTH_TEST);
        }

        void setForGridRendering() {
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LEQUAL);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_TRUE);
        }

        void setForWireframe() {
            glDisable(GL_CULL_FACE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glLineWidth(3.0f);
        }

        void setForSolidFaces() {
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LESS);
            glDepthMask(GL_TRUE);
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            glFrontFace(GL_CCW);
            glDisable(GL_BLEND);
        }

        void setForViewCube() {
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LEQUAL);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
        }

    private:
        void setEnabled(GLenum cap, GLboolean enabled) const {
            if (enabled) {
                glEnable(cap);
            } else {
                glDisable(cap);
            }
        }
    };

    // Global instance for convenience
    inline OpenGLStateManager& getGLStateManager() {
        static OpenGLStateManager instance;
        return instance;
    }

} // namespace gs
