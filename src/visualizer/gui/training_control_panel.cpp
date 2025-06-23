#include "visualizer/gui/training_control_panel.hpp"
#include "core/trainer.hpp"
#include <algorithm>
#include <cuda_runtime.h>

namespace gs {

    TrainingControlPanel::TrainingControlPanel(Trainer* trainer, std::shared_ptr<TrainingInfo> info)
        : GUIPanel("Training Control"),
          trainer_(trainer),
          info_(info) {
    }

    void TrainingControlPanel::render() {
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.5f, 0.5f, 0.5f, 0.8f));

        ImGui::Begin(title_.c_str(), &visible_, window_flags_);
        ImGui::SetWindowSize(ImVec2(300, 0));

        window_active_ = ImGui::IsWindowHovered();

        if (!trainer_) {
            ImGui::Text("Waiting for trainer initialization...");
            ImGui::End();
            ImGui::PopStyleColor();
            return;
        }

        renderControlButtons();
        ImGui::Separator();
        renderStatusDisplay();
        ImGui::Separator();
        renderProgressBar();
        renderLossPlot();
        renderGPUUsage();

        ImGui::Text("Num Splats: %d", info_->num_splats_);

        ImGui::End();
        ImGui::PopStyleColor();
    }

    void TrainingControlPanel::renderControlButtons() {
        ImGui::Text("Training Control");
        ImGui::Separator();

        bool is_training = trainer_->is_running();
        bool is_paused = trainer_->is_paused();
        bool is_complete = trainer_->is_training_complete();
        bool has_stopped = trainer_->has_stopped();

        if (!training_started_ && !is_training) {
            // Initial state - show start button
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
            if (ImGui::Button("Start Training", ImVec2(-1, 0))) {
                start_triggered_ = true;
                training_started_ = true;
            }
            ImGui::PopStyleColor(2);
        } else if (is_complete || has_stopped) {
            // Training finished - show status
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f),
                               has_stopped ? "Training Stopped!" : "Training Complete!");
        } else {
            // Training in progress - show control buttons
            if (is_paused) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
                if (ImGui::Button("Resume", ImVec2(-1, 0))) {
                    trainer_->request_resume();
                }
                ImGui::PopStyleColor(2);

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

            // Save checkpoint button
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
    }

    void TrainingControlPanel::renderStatusDisplay() {
        int current_iter = trainer_->get_current_iteration();
        float current_loss = trainer_->get_current_loss();
        bool is_training = trainer_->is_running();
        bool is_paused = trainer_->is_paused();
        bool is_complete = trainer_->is_training_complete();

        ImGui::Text("Status: %s",
                    is_complete ? "Complete" : (is_paused ? "Paused" : (is_training ? "Training" : "Ready")));
        ImGui::Text("Iteration: %d", current_iter);
        ImGui::Text("Loss: %.6f", current_loss);

#ifdef CUDA_GL_INTEROP_ENABLED
        ImGui::Text("Render Mode: GPU Direct (Interop)");
#else
        ImGui::Text("Render Mode: CPU Copy");
#endif
    }

    void TrainingControlPanel::renderProgressBar() {
        int current_iter;
        int total_iter;
        {
            std::lock_guard<std::mutex> lock(info_->mtx);
            current_iter = info_->curr_iterations_;
            total_iter = info_->total_iterations_;
        }

        float fraction = total_iter > 0 ? float(current_iter) / float(total_iter) : 0.0f;
        char overlay_text[64];
        std::snprintf(overlay_text, sizeof(overlay_text), "%d / %d", current_iter, total_iter);
        ImGui::ProgressBar(fraction, ImVec2(-1, 20), overlay_text);
    }

    void TrainingControlPanel::renderLossPlot() {
        std::vector<float> loss_data;
        {
            std::lock_guard<std::mutex> lock(info_->mtx);
            loss_data.assign(info_->loss_buffer_.begin(), info_->loss_buffer_.end());
        }

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
    }

    void TrainingControlPanel::renderGPUUsage() {
        float gpuUsage = getGPUUsage();
        char gpuText[64];
        std::snprintf(gpuText, sizeof(gpuText), "GPU Usage: %.1f%%", gpuUsage);
        ImGui::ProgressBar(gpuUsage / 100.0f, ImVec2(-1, 20), gpuText);
    }

    float TrainingControlPanel::getGPUUsage() {
        size_t free_byte, total_byte;
        cudaDeviceSynchronize();
        cudaMemGetInfo(&free_byte, &total_byte);
        size_t used_byte = total_byte - free_byte;
        return used_byte / (float)total_byte * 100;
    }

} // namespace gs