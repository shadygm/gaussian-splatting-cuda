#pragma once

#include "visualizer/gui/gui_manager.hpp"
#include <chrono>
#include <deque>
#include <mutex>

namespace gs {

    class TrainingControlPanel : public GUIPanel {
    public:
        struct TrainingState {
            bool is_training = false;
            bool is_paused = false;
            bool is_complete = false;
            bool has_stopped = false;
            int current_iteration = 0;
            float current_loss = 0.0f;
        };

        struct TrainingInfo {
            std::mutex mtx;
            int curr_iterations_ = 0;
            int total_iterations_ = 0;
            int num_splats_ = 0;
            int max_loss_points_ = 200;
            std::deque<float> loss_buffer_;

            void updateProgress(int iter, int total_iterations) {
                curr_iterations_ = iter;
                total_iterations_ = total_iterations;
            }

            void updateNumSplats(int num_splats) {
                num_splats_ = num_splats;
            }

            void updateLoss(float loss) {
                loss_buffer_.push_back(loss);
                while (loss_buffer_.size() > max_loss_points_) {
                    loss_buffer_.pop_front();
                }
            }
        };

        TrainingControlPanel(Trainer* trainer, std::shared_ptr<TrainingInfo> info);

        void render() override;

        // Get control states
        bool shouldStartTraining() const { return start_triggered_; }
        void resetStartTrigger() { start_triggered_ = false; }

    private:
        void renderControlButtons();
        void renderStatusDisplay();
        void renderProgressBar();
        void renderLossPlot();
        void renderGPUUsage();

        Trainer* trainer_;
        std::shared_ptr<TrainingInfo> info_;

        // Control states
        bool training_started_ = false;
        bool start_triggered_ = false;
        bool save_in_progress_ = false;
        std::chrono::steady_clock::time_point save_start_time_;

        float getGPUUsage();
    };

} // namespace gs