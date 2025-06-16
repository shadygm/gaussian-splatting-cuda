#include "core/mcmc.hpp"
#include "Ops.h"
#include "core/debug_utils.hpp"
#include "core/parameters.hpp"
#include "core/rasterizer.hpp"
#include <c10/cuda/CUDACachingAllocator.h>
#include <iostream>
#include <random>

namespace {

    // Constants
    constexpr int64_t MULTINOMIAL_LIMIT = 1 << 24;
    constexpr float MIN_OPACITY_CLAMP = 1e-7f;
    constexpr float NOISE_K = 100.0f;
    constexpr float NOISE_X0 = 0.995f;

    // Helper to handle opacity tensor shapes consistently
    torch::Tensor normalize_opacity_shape(const torch::Tensor& opacity) {
        return (opacity.dim() == 2 && opacity.size(1) == 1) ? opacity.squeeze(-1) : opacity;
    }

    // Multinomial sampling implementation
    torch::Tensor multinomial_sample(const torch::Tensor& weights, const int n, const bool replacement = true) {
        const int64_t num_elements = weights.size(0);

        if (num_elements <= MULTINOMIAL_LIMIT) {
            return torch::multinomial(weights, n, replacement);
        }

        // For larger arrays, implement sampling manually
        const auto weights_normalized = weights / weights.sum();
        const auto weights_cpu = weights_normalized.cpu();
        const auto cumsum = weights_cpu.cumsum(0);
        const auto cumsum_data = cumsum.accessor<float, 1>();

        std::vector<int64_t> sampled_indices;
        sampled_indices.reserve(n);

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(0.0, 1.0);

        for (int i = 0; i < n; ++i) {
            const float u = dis(gen);
            // Binary search
            int64_t idx = 0;
            int64_t left = 0, right = num_elements - 1;
            while (left <= right) {
                const int64_t mid = (left + right) / 2;
                if (cumsum_data[mid] < u) {
                    left = mid + 1;
                } else {
                    idx = mid;
                    right = mid - 1;
                }
            }
            sampled_indices.push_back(idx);
        }

        return torch::tensor(sampled_indices, torch::kLong).to(weights.device());
    }

    // Reset Adam optimizer state for specific indices
    void reset_adam_state_at_indices(torch::optim::Adam* optimizer,
                                     const torch::Tensor& indices,
                                     const int param_position) {
        auto& param = optimizer->param_groups()[param_position].params()[0];
        void* const param_key = param.unsafeGetTensorImpl();

        const auto state_it = optimizer->state().find(param_key);
        if (state_it == optimizer->state().end()) {
            return; // No state exists yet
        }

        auto& adam_state = static_cast<torch::optim::AdamParamState&>(*state_it->second);
        adam_state.exp_avg().index_put_({indices}, 0);
        adam_state.exp_avg_sq().index_put_({indices}, 0);

        if (adam_state.max_exp_avg_sq().defined()) {
            adam_state.max_exp_avg_sq().index_put_({indices}, 0);
        }
    }

    // Calculate ratios for relocation
    torch::Tensor calculate_relocation_ratios(const torch::Tensor& sampled_indices,
                                              const torch::Tensor& opacities,
                                              const int n_max) {
        auto ratios = torch::zeros({opacities.size(0)}, torch::kFloat32).to(torch::kCUDA);
        ratios.index_add_(0, sampled_indices, torch::ones_like(sampled_indices, torch::kFloat32));
        ratios = ratios.index_select(0, sampled_indices) + 1;
        return torch::clamp(ratios, 1, n_max).to(torch::kInt32).contiguous();
    }

    // Update opacity raw tensor based on its dimensionality
    void update_opacity_raw(torch::Tensor& opacity_raw,
                            const torch::Tensor& indices,
                            const torch::Tensor& new_opacities,
                            const float min_opacity) {
        const auto clamped = torch::clamp(new_opacities, min_opacity, 1.0f - MIN_OPACITY_CLAMP);
        if (opacity_raw.dim() == 2) {
            opacity_raw.index_put_({indices, torch::indexing::Slice()},
                                   torch::logit(clamped).unsqueeze(-1));
        } else {
            opacity_raw.index_put_({indices}, torch::logit(clamped));
        }
    }

    // Initialize binomial coefficients
    torch::Tensor init_binomial_coefficients(const int n_max, const torch::Device device) {
        auto binoms = torch::zeros({n_max, n_max}, torch::kFloat32);
        auto binoms_accessor = binoms.accessor<float, 2>();

        for (int n = 0; n < n_max; ++n) {
            for (int k = 0; k <= n; ++k) {
                float binom = 1.0f;
                for (int i = 0; i < k; ++i) {
                    binom *= static_cast<float>(n - i) / static_cast<float>(i + 1);
                }
                binoms_accessor[n][k] = binom;
            }
        }
        return binoms.to(device);
    }

    // Extend Adam optimizer state for concatenated parameters
    std::unique_ptr<torch::optim::AdamParamState> extend_adam_state(
        const torch::optim::AdamParamState& old_state,
        const torch::IntArrayRef new_shape) {

        const auto zeros_to_add = torch::zeros(new_shape, old_state.exp_avg().options());

        auto new_state = std::make_unique<torch::optim::AdamParamState>();
        new_state->step(old_state.step());
        new_state->exp_avg(torch::cat({old_state.exp_avg(), zeros_to_add}, 0));
        new_state->exp_avg_sq(torch::cat({old_state.exp_avg_sq(), zeros_to_add}, 0));

        if (old_state.max_exp_avg_sq().defined()) {
            new_state->max_exp_avg_sq(torch::cat({old_state.max_exp_avg_sq(), zeros_to_add}, 0));
        }

        return new_state;
    }

} // anonymous namespace

MCMC::MCMC(SplatData&& splat_data) : _splat_data(std::move(splat_data)) {}

void MCMC::ExponentialLR::step() {
    auto& group = optimizer_.param_groups()[param_group_index_];
    auto& options = static_cast<torch::optim::AdamOptions&>(group.options());
    options.lr(options.lr() * gamma_);
}

int MCMC::relocate_gs() {
    torch::NoGradGuard no_grad;

    const auto opacities = normalize_opacity_shape(_splat_data.get_opacity());
    const auto dead_mask = opacities <= _params->min_opacity;
    const auto dead_indices = dead_mask.nonzero().squeeze(-1);

    if (dead_indices.numel() == 0)
        return 0;

    const auto alive_mask = ~dead_mask;
    const auto alive_indices = alive_mask.nonzero().squeeze(-1);

    if (alive_indices.numel() == 0)
        return 0;

    // Sample from alive Gaussians
    const auto probs = opacities.index_select(0, alive_indices);
    const auto sampled_idxs_local = multinomial_sample(probs, dead_indices.numel(), true);
    const auto sampled_idxs = alive_indices.index_select(0, sampled_idxs_local);

    // Calculate ratios for relocation
    const auto ratios = calculate_relocation_ratios(sampled_idxs, opacities, _binoms.size(0));

    // Relocate using gsplat
    const auto [new_opacities, new_scales] = gsplat::relocation(
        opacities.index_select(0, sampled_idxs),
        _splat_data.get_scaling().index_select(0, sampled_idxs),
        ratios,
        _binoms,
        _binoms.size(0));

    // Update parameters for sampled indices
    update_opacity_raw(_splat_data.opacity_raw(), sampled_idxs, new_opacities, _params->min_opacity);
    _splat_data.scaling_raw().index_put_({sampled_idxs}, torch::log(new_scales));

    // Copy from sampled to dead indices
    auto copy_param = [&](torch::Tensor& param) {
        param.index_put_({dead_indices}, param.index_select(0, sampled_idxs));
    };

    copy_param(_splat_data.means());
    copy_param(_splat_data.sh0());
    copy_param(_splat_data.shN());
    copy_param(_splat_data.scaling_raw());
    copy_param(_splat_data.rotation_raw());
    copy_param(_splat_data.opacity_raw());

    // Reset optimizer states for all parameters
    for (int i = 0; i < 6; ++i) {
        reset_adam_state_at_indices(_optimizer.get(), sampled_idxs, i);
    }

    return dead_indices.numel();
}

int MCMC::add_new_gs() {
    torch::NoGradGuard no_grad;

    if (!_optimizer) {
        std::cerr << "Warning: add_new_gs called but optimizer not initialized" << std::endl;
        return 0;
    }

    const int current_n = _splat_data.size();
    const int n_target = std::min(_params->max_cap, static_cast<int>(1.05f * current_n));
    const int n_new = std::max(0, n_target - current_n);

    if (n_new == 0)
        return 0;

    const auto opacities = normalize_opacity_shape(_splat_data.get_opacity());
    const auto sampled_idxs = multinomial_sample(opacities.flatten(), n_new, true);

    // Calculate ratios for relocation
    const auto ratios = calculate_relocation_ratios(sampled_idxs, opacities, _binoms.size(0));

    // Relocate using gsplat
    const auto [new_opacities, new_scales] = gsplat::relocation(
        opacities.index_select(0, sampled_idxs),
        _splat_data.get_scaling().index_select(0, sampled_idxs),
        ratios,
        _binoms,
        _binoms.size(0));

    // Update existing Gaussians first
    update_opacity_raw(_splat_data.opacity_raw(), sampled_idxs, new_opacities, _params->min_opacity);
    _splat_data.scaling_raw().index_put_({sampled_idxs}, torch::log(new_scales));

    // Prepare concatenated parameters
    struct ParamPair {
        torch::Tensor* model_param;
        torch::Tensor concatenated;
    };

    std::array<ParamPair, 6> param_pairs = {{{&_splat_data.means(), torch::cat({_splat_data.means(), _splat_data.means().index_select(0, sampled_idxs)}, 0).set_requires_grad(true)},
                                             {&_splat_data.sh0(), torch::cat({_splat_data.sh0(), _splat_data.sh0().index_select(0, sampled_idxs)}, 0).set_requires_grad(true)},
                                             {&_splat_data.shN(), torch::cat({_splat_data.shN(), _splat_data.shN().index_select(0, sampled_idxs)}, 0).set_requires_grad(true)},
                                             {&_splat_data.scaling_raw(), torch::cat({_splat_data.scaling_raw(), _splat_data.scaling_raw().index_select(0, sampled_idxs)}, 0).set_requires_grad(true)},
                                             {&_splat_data.rotation_raw(), torch::cat({_splat_data.rotation_raw(), _splat_data.rotation_raw().index_select(0, sampled_idxs)}, 0).set_requires_grad(true)},
                                             {&_splat_data.opacity_raw(), torch::cat({_splat_data.opacity_raw(), _splat_data.opacity_raw().index_select(0, sampled_idxs)}, 0).set_requires_grad(true)}}};

    // Update optimizer states
    std::vector<void*> old_param_keys;
    std::vector<std::unique_ptr<torch::optim::OptimizerParamState>> saved_states;

    for (int i = 0; i < 6; ++i) {
        auto& old_param = _optimizer->param_groups()[i].params()[0];
        void* const old_param_key = old_param.unsafeGetTensorImpl();
        old_param_keys.push_back(old_param_key);

        const auto state_it = _optimizer->state().find(old_param_key);
        if (state_it != _optimizer->state().end()) {
            const auto& adam_state = static_cast<torch::optim::AdamParamState&>(*state_it->second);

            // Build shape for new elements based on actual tensor dimensions
            std::vector<int64_t> new_shape;
            new_shape.push_back(n_new);
            for (int64_t d = 1; d < param_pairs[i].concatenated.dim(); ++d) {
                new_shape.push_back(param_pairs[i].concatenated.size(d));
            }

            saved_states.push_back(extend_adam_state(adam_state, new_shape));
        } else {
            saved_states.push_back(nullptr);
        }
    }

    // Clear old states
    for (const auto key : old_param_keys) {
        _optimizer->state().erase(key);
    }

    // Update parameters and states
    for (int i = 0; i < 6; ++i) {
        _optimizer->param_groups()[i].params()[0] = param_pairs[i].concatenated;
        *param_pairs[i].model_param = param_pairs[i].concatenated;

        if (saved_states[i]) {
            void* const new_param_key = param_pairs[i].concatenated.unsafeGetTensorImpl();
            _optimizer->state()[new_param_key] = std::move(saved_states[i]);
        }
    }

    return n_new;
}

void MCMC::inject_noise() {
    torch::NoGradGuard no_grad;

    const auto opacities = normalize_opacity_shape(_splat_data.get_opacity());
    const auto quats = _splat_data.get_rotation();
    const auto scales = _splat_data.get_scaling();

    // Get covariance matrices
    const auto [covars, _] = gsplat::quat_scale_to_covar_preci_fwd(
        quats, scales, true, false, false);

    // Opacity-based sigmoid weighting
    const auto op_sigmoid = 1.0f / (1.0f + torch::exp(-NOISE_K * ((1.0f - opacities) - NOISE_X0)));

    // Get current learning rate
    const float current_lr = static_cast<torch::optim::AdamOptions&>(
                                 _optimizer->param_groups()[0].options())
                                 .lr();

    // Generate and apply noise
    auto noise = torch::randn_like(_splat_data.means()) * op_sigmoid.unsqueeze(-1) * current_lr * NOISE_LR;
    noise = torch::bmm(covars, noise.unsqueeze(-1)).squeeze(-1);
    _splat_data.means().add_(noise);
}

void MCMC::post_backward(const int iter, gs::RenderOutput& render_output) {
    torch::NoGradGuard no_grad;

    // Increment SH degree periodically
    if (iter % 1000 == 0) {
        _splat_data.increment_sh_degree();
    }

    // Refine Gaussians
    if (is_refining(iter)) {
        relocate_gs();
        add_new_gs();
        c10::cuda::CUDACachingAllocator::emptyCache();
    }

    inject_noise();
}

void MCMC::step(const int iter) {
    if (iter < _params->iterations) {
        _optimizer->step();
        _optimizer->zero_grad(true);
        _scheduler->step();
    }
}

void MCMC::initialize(const gs::param::OptimizationParameters& optimParams) {
    _params = std::make_unique<const gs::param::OptimizationParameters>(optimParams);

    // Move all parameters to GPU and enable gradients
    const auto dev = torch::kCUDA;
    _splat_data.means() = _splat_data.means().to(dev).set_requires_grad(true);
    _splat_data.scaling_raw() = _splat_data.scaling_raw().to(dev).set_requires_grad(true);
    _splat_data.rotation_raw() = _splat_data.rotation_raw().to(dev).set_requires_grad(true);
    _splat_data.opacity_raw() = _splat_data.opacity_raw().to(dev).set_requires_grad(true);
    _splat_data.sh0() = _splat_data.sh0().to(dev).set_requires_grad(true);
    _splat_data.shN() = _splat_data.shN().to(dev).set_requires_grad(true);

    // Initialize binomial coefficients
    _binoms = init_binomial_coefficients(BINOMIAL_MAX_N, dev);

    // Initialize optimizer with parameter groups
    using torch::optim::AdamOptions;
    std::vector<torch::optim::OptimizerParamGroup> groups = {
        {{_splat_data.means()}, std::make_unique<AdamOptions>(_params->means_lr * _splat_data.get_scene_scale())},
        {{_splat_data.sh0()}, std::make_unique<AdamOptions>(_params->shs_lr)},
        {{_splat_data.shN()}, std::make_unique<AdamOptions>(_params->shs_lr / 20.f)},
        {{_splat_data.scaling_raw()}, std::make_unique<AdamOptions>(_params->scaling_lr)},
        {{_splat_data.rotation_raw()}, std::make_unique<AdamOptions>(_params->rotation_lr)},
        {{_splat_data.opacity_raw()}, std::make_unique<AdamOptions>(_params->opacity_lr)}};

    // Set epsilon for all groups
    for (auto& g : groups) {
        static_cast<AdamOptions&>(g.options()).eps(1e-15);
    }

    _optimizer = std::make_unique<torch::optim::Adam>(groups, AdamOptions(0.f).eps(1e-15));

    // Initialize scheduler
    const double gamma = std::pow(0.01, 1.0 / _params->iterations);
    _scheduler = std::make_unique<ExponentialLR>(*_optimizer, gamma, 0);
}

bool MCMC::is_refining(int iter) const {
    return iter < _params->stop_refine &&
           iter > _params->start_refine &&
           iter % _params->refine_every == 0;
}