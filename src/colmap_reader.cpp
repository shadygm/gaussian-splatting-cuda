#include "core/colmap_reader.hpp"
#include "core/point_cloud.hpp"
#include "core/torch_shapes.hpp"
#include "core/image_io.hpp"  // Added for load_image
#include <algorithm>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <torch/torch.h>
#include <unordered_map>
#include <vector>

namespace F = torch::nn::functional;

// -----------------------------------------------------------------------------
//  Quaternion to rotation matrix
// -----------------------------------------------------------------------------
inline torch::Tensor qvec2rotmat(const torch::Tensor& qraw) {
    assert_vec(qraw, 4, "qvec");

    auto q = F::normalize(qraw.to(torch::kFloat32),
                          F::NormalizeFuncOptions().dim(0));

    auto w = q[0], x = q[1], y = q[2], z = q[3];

    torch::Tensor R = torch::empty({3, 3}, torch::kFloat32);
    R[0][0] = 1 - 2 * (y * y + z * z);
    R[0][1] = 2 * (x * y - z * w);
    R[0][2] = 2 * (x * z + y * w);

    R[1][0] = 2 * (x * y + z * w);
    R[1][1] = 1 - 2 * (x * x + z * z);
    R[1][2] = 2 * (y * z - x * w);

    R[2][0] = 2 * (x * z - y * w);
    R[2][1] = 2 * (y * z + x * w);
    R[2][2] = 1 - 2 * (x * x + y * y);
    return R;
}

inline float focal2fov(float focal, int pixels) {
    return 2.0f * std::atan(pixels / (2.0f * focal));
}

class Image {
public:
    Image() = default;
    explicit Image(uint32_t id)
        : _image_ID(id) {}

    uint32_t _camera_id = 0;
    std::string _name;

    torch::Tensor _qvec = torch::tensor({1.f, 0.f, 0.f, 0.f}, torch::kFloat32);
    torch::Tensor _tvec = torch::zeros({3}, torch::kFloat32);

private:
    uint32_t _image_ID = 0;
};

// -----------------------------------------------------------------------------
//  Build 4x4 world-to-camera matrix
// -----------------------------------------------------------------------------
inline torch::Tensor getWorld2View(const torch::Tensor& R,
                                   const torch::Tensor& T) {
    assert_mat(R, 3, 3, "R");
    assert_vec(T, 3, "T");

    torch::Tensor M = torch::eye(4, torch::kFloat32);
    M.index_put_({torch::indexing::Slice(0, 3),
                  torch::indexing::Slice(0, 3)},
                 R);
    M.index_put_({torch::indexing::Slice(0, 3), 3},
                 (-torch::matmul(R, T)).reshape({3}));
    return M;
}

// -----------------------------------------------------------------------------
//  POD read helpers
// -----------------------------------------------------------------------------
static inline uint64_t read_u64(const char*& p) {
    uint64_t v;
    std::memcpy(&v, p, 8);
    p += 8;
    return v;
}
static inline uint32_t read_u32(const char*& p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    p += 4;
    return v;
}
static inline int32_t read_i32(const char*& p) {
    int32_t v;
    std::memcpy(&v, p, 4);
    p += 4;
    return v;
}
static inline double read_f64(const char*& p) {
    double v;
    std::memcpy(&v, p, 8);
    p += 8;
    return v;
}

// -----------------------------------------------------------------------------
//  COLMAP camera-model map
// -----------------------------------------------------------------------------
static const std::unordered_map<int, std::pair<CAMERA_MODEL, int32_t>> camera_model_ids = {
    {0, {CAMERA_MODEL::SIMPLE_PINHOLE, 3}},
    {1, {CAMERA_MODEL::PINHOLE, 4}},
    {2, {CAMERA_MODEL::SIMPLE_RADIAL, 4}},
    {3, {CAMERA_MODEL::RADIAL, 5}},
    {4, {CAMERA_MODEL::OPENCV, 8}},
    {5, {CAMERA_MODEL::OPENCV_FISHEYE, 8}},
    {6, {CAMERA_MODEL::FULL_OPENCV, 12}},
    {7, {CAMERA_MODEL::FOV, 5}},
    {8, {CAMERA_MODEL::SIMPLE_RADIAL_FISHEYE, 4}},
    {9, {CAMERA_MODEL::RADIAL_FISHEYE, 5}},
    {10, {CAMERA_MODEL::THIN_PRISM_FISHEYE, 12}},
    {11, {CAMERA_MODEL::UNDEFINED, -1}}};

// -----------------------------------------------------------------------------
//  Binary-file loader
// -----------------------------------------------------------------------------
static std::unique_ptr<std::vector<char>>
read_binary(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    if (!f)
        throw std::runtime_error("Failed to open " + p.string());

    auto sz = static_cast<std::streamsize>(f.tellg());
    auto buf = std::make_unique<std::vector<char>>(static_cast<size_t>(sz));

    f.seekg(0, std::ios::beg);
    f.read(buf->data(), sz);
    if (!f)
        throw std::runtime_error("Short read on " + p.string());
    return buf;
}

// -----------------------------------------------------------------------------
//  images.bin
// -----------------------------------------------------------------------------
std::vector<Image> read_images_binary(const std::filesystem::path& file_path) {
    auto buf_owner = read_binary(file_path);
    const char* cur = buf_owner->data();
    const char* end = cur + buf_owner->size();

    uint64_t n_images = read_u64(cur);
    std::vector<Image> images;
    images.reserve(n_images);

    for (uint64_t i = 0; i < n_images; ++i) {
        uint32_t id = read_u32(cur);
        auto& img = images.emplace_back(id);

        torch::Tensor q = torch::empty({4}, torch::kFloat32);
        for (int k = 0; k < 4; ++k)
            q[k] = static_cast<float>(read_f64(cur));

        img._qvec = q;

        torch::Tensor t = torch::empty({3}, torch::kFloat32);
        for (int k = 0; k < 3; ++k)
            t[k] = static_cast<float>(read_f64(cur));
        img._tvec = t;

        img._camera_id = read_u32(cur);

        img._name.assign(cur);
        cur += img._name.size() + 1; // skip '\0'

        uint64_t npts = read_u64(cur); // skip 2-D points
        cur += npts * (sizeof(double) * 2 + sizeof(uint64_t));
    }
    if (cur != end)
        throw std::runtime_error("images.bin: trailing bytes");
    return images;
}

// -----------------------------------------------------------------------------
//  cameras.bin
// -----------------------------------------------------------------------------
std::unordered_map<uint32_t, CameraData>
read_cameras_binary(const std::filesystem::path& file_path, float factor = 1.0f) {
    auto buf_owner = read_binary(file_path);
    const char* cur = buf_owner->data();
    const char* end = cur + buf_owner->size();

    uint64_t n_cams = read_u64(cur);
    std::unordered_map<uint32_t, CameraData> cams;
    cams.reserve(n_cams);

    for (uint64_t i = 0; i < n_cams; ++i) {
        CameraData cam;
        cam._camera_ID = read_u32(cur);

        int32_t model_id = read_i32(cur);
        cam._width = read_u64(cur);
        cam._height = read_u64(cur);

        // Apply downscaling factor to dimensions
        cam._width = static_cast<uint64_t>(cam._width / factor);
        cam._height = static_cast<uint64_t>(cam._height / factor);

        auto it = camera_model_ids.find(model_id);
        if (it == camera_model_ids.end() || it->second.second < 0)
            throw std::runtime_error("Unsupported camera-model id " + std::to_string(model_id));

        cam._camera_model = it->second.first;
        int32_t param_cnt = it->second.second;

        // Read parameters and apply scaling
        std::vector<double> raw_params(param_cnt);
        for (int j = 0; j < param_cnt; j++) {
            raw_params[j] = read_f64(cur);
        }

        // Scale parameters based on camera model
        switch (cam._camera_model) {
        case CAMERA_MODEL::SIMPLE_PINHOLE: {
            // params: [f, cx, cy]
            raw_params[0] /= factor;  // f
            raw_params[1] /= factor;  // cx
            raw_params[2] /= factor;  // cy
            break;
        }
        case CAMERA_MODEL::PINHOLE: {
            // params: [fx, fy, cx, cy]
            raw_params[0] /= factor;  // fx
            raw_params[1] /= factor;  // fy
            raw_params[2] /= factor;  // cx
            raw_params[3] /= factor;  // cy
            break;
        }
        case CAMERA_MODEL::SIMPLE_RADIAL:
        case CAMERA_MODEL::RADIAL: {
            // params: [f, cx, cy, k1, ...] or [fx, fy, cx, cy, k1, ...]
            raw_params[0] /= factor;  // f or fx
            if (cam._camera_model == CAMERA_MODEL::RADIAL) {
                raw_params[1] /= factor;  // fy
                raw_params[2] /= factor;  // cx
                raw_params[3] /= factor;  // cy
            } else {
                raw_params[1] /= factor;  // cx
                raw_params[2] /= factor;  // cy
            }
            // Distortion parameters remain unchanged
            break;
        }
        // Add more camera models as needed
        default:
            // For unsupported models, at least scale principal point
            if (param_cnt >= 4) {
                raw_params[2] /= factor;  // cx
                raw_params[3] /= factor;  // cy
            }
            break;
        }

        cam._params = torch::from_blob(raw_params.data(), {param_cnt}, torch::kFloat64)
                          .clone()
                          .to(torch::kFloat32);

        cams.emplace(cam._camera_ID, std::move(cam));
    }
    if (cur != end)
        throw std::runtime_error("cameras.bin: trailing bytes");
    return cams;
}

// -----------------------------------------------------------------------------
//  points3D.bin
// -----------------------------------------------------------------------------
PointCloud read_point3D_binary(const std::filesystem::path& file_path) {
    auto buf_owner = read_binary(file_path);
    const char* cur = buf_owner->data();
    const char* end = cur + buf_owner->size();

    uint64_t N = read_u64(cur);

    // Pre-allocate tensors directly
    torch::Tensor positions = torch::empty({static_cast<int64_t>(N), 3}, torch::kFloat32);
    torch::Tensor colors = torch::empty({static_cast<int64_t>(N), 3}, torch::kUInt8);

    // Get raw pointers for efficient access
    float* pos_data = positions.data_ptr<float>();
    uint8_t* col_data = colors.data_ptr<uint8_t>();

    for (uint64_t i = 0; i < N; ++i) {
        cur += 8; // skip point ID

        // Read position directly into tensor
        pos_data[i * 3 + 0] = static_cast<float>(read_f64(cur));
        pos_data[i * 3 + 1] = static_cast<float>(read_f64(cur));
        pos_data[i * 3 + 2] = static_cast<float>(read_f64(cur));

        // Read color directly into tensor
        col_data[i * 3 + 0] = *cur++;
        col_data[i * 3 + 1] = *cur++;
        col_data[i * 3 + 2] = *cur++;

        cur += 8;                                    // skip reprojection error
        cur += read_u64(cur) * sizeof(uint32_t) * 2; // skip track
    }

    if (cur != end)
        throw std::runtime_error("points3D.bin: trailing bytes");

    return PointCloud(positions, colors);
}

// -----------------------------------------------------------------------------
//  Extract downscaling factor from folder name
// -----------------------------------------------------------------------------
float get_downscale_factor(const std::string& images_folder) {
    // Check if folder name ends with _N where N is a number
    size_t underscore_pos = images_folder.rfind('_');
    if (underscore_pos != std::string::npos) {
        std::string suffix = images_folder.substr(underscore_pos + 1);
        try {
            return std::stof(suffix);
        } catch (...) {
            // Not a valid number, no downscaling
        }
    }
    return 1.0f;
}

// -----------------------------------------------------------------------------
//  Assemble per-image camera information
// -----------------------------------------------------------------------------
std::tuple<std::vector<CameraData>, float>
read_colmap_cameras(const std::filesystem::path base_path,
                    const std::unordered_map<uint32_t, CameraData>& cams,
                    const std::vector<Image>& images,
                    const std::string& images_folder,
                    int resolution) {
    std::vector<CameraData> out(images.size());

    std::filesystem::path images_path = base_path / images_folder;

    // Prepare tensor to store all camera locations [N, 3]
    torch::Tensor camera_locations = torch::zeros({static_cast<int64_t>(images.size()), 3}, torch::kFloat32);

    // Check if the specified images folder exists
    if (!std::filesystem::exists(images_path)) {
        throw std::runtime_error("Images folder does not exist: " + images_path.string());
    }

    for (size_t i = 0; i < images.size(); ++i) {
        const Image& img = images[i];
        auto it = cams.find(img._camera_id);
        if (it == cams.end())
            throw std::runtime_error("Camera ID " + std::to_string(img._camera_id) + " not found");

        out[i] = it->second;
        out[i]._image_path = images_path / img._name;
        out[i]._image_name = img._name;

        out[i]._R = qvec2rotmat(img._qvec);
        out[i]._T = img._tvec.clone();

        // Camera location in world space = -R^T * T
        camera_locations[i] = -torch::matmul(out[i]._R.t(), out[i]._T);

        // Initialize image dimensions
        out[i]._img_w = out[i]._img_h = out[i]._channels = 0;
        out[i]._img_data = nullptr;
    }

    // Check actual image dimensions and adjust intrinsics if needed
    if (!out.empty() && std::filesystem::exists(out[0]._image_path)) {
        // Load first image to get actual dimensions
        int actual_width, actual_height, channels;
        unsigned char* img_data;

        // If resolution is specified, images will be resized to that resolution
        if (resolution > 0) {
            std::tie(img_data, actual_width, actual_height, channels) =
                load_image(out[0]._image_path, resolution);
        } else {
            std::tie(img_data, actual_width, actual_height, channels) =
                load_image(out[0]._image_path, -1);  // -1 means no resize
        }

        // Get expected dimensions from camera params
        int expected_width = out[0]._width;
        int expected_height = out[0]._height;

        // Calculate additional scaling factors
        float s_width = static_cast<float>(actual_width) / expected_width;
        float s_height = static_cast<float>(actual_height) / expected_height;

        // Free the image data
        free_image(img_data);

        std::cout << "Image dimension adjustment:" << std::endl;
        std::cout << "  Actual: " << actual_width << "x" << actual_height << std::endl;
        std::cout << "  Expected (after factor): " << expected_width << "x" << expected_height << std::endl;
        std::cout << "  Additional scale factors: " << s_width << ", " << s_height << std::endl;

        // Apply scaling to all cameras if there's a mismatch
        if (std::abs(s_width - 1.0f) > 1e-5 || std::abs(s_height - 1.0f) > 1e-5) {
            for (auto& cam : out) {
                // Update intrinsics based on camera model
                switch (cam._camera_model) {
                case CAMERA_MODEL::SIMPLE_PINHOLE: {
                    // SIMPLE_PINHOLE params: [f, cx, cy]
                    float f = cam._params[0].item<float>() * s_width;
                    float cx = cam._params[1].item<float>() * s_width;
                    float cy = cam._params[2].item<float>() * s_height;

                    cam._params = torch::tensor({f, cx, cy}, torch::kFloat32);
                    break;
                }
                case CAMERA_MODEL::PINHOLE: {
                    // PINHOLE params: [fx, fy, cx, cy]
                    float fx = cam._params[0].item<float>() * s_width;
                    float fy = cam._params[1].item<float>() * s_height;
                    float cx = cam._params[2].item<float>() * s_width;
                    float cy = cam._params[3].item<float>() * s_height;

                    cam._params = torch::tensor({fx, fy, cx, cy}, torch::kFloat32);
                    break;
                }
                default:
                    throw std::runtime_error("Unsupported camera model for scaling adjustment");
                }

                // Update stored dimensions to actual dimensions
                cam._width = actual_width;
                cam._height = actual_height;
            }
        }
    }

    // Now compute FOV and K matrices for all cameras with the final dimensions
    for (auto& cam : out) {
        // Compute K matrix
        torch::Tensor K = torch::zeros({3, 3}, torch::kFloat32);

        switch (cam._camera_model) {
        case CAMERA_MODEL::SIMPLE_PINHOLE: {
            float f = cam._params[0].item<float>();
            float cx = cam._params[1].item<float>();
            float cy = cam._params[2].item<float>();

            K[0][0] = f;
            K[1][1] = f;
            K[0][2] = cx;
            K[1][2] = cy;
            K[2][2] = 1.0f;

            cam._fov_x = focal2fov(f, cam._width);
            cam._fov_y = focal2fov(f, cam._height);
            break;
        }
        case CAMERA_MODEL::PINHOLE: {
            float fx = cam._params[0].item<float>();
            float fy = cam._params[1].item<float>();
            float cx = cam._params[2].item<float>();
            float cy = cam._params[3].item<float>();

            K[0][0] = fx;
            K[1][1] = fy;
            K[0][2] = cx;
            K[1][2] = cy;
            K[2][2] = 1.0f;

            cam._fov_x = focal2fov(fx, cam._width);
            cam._fov_y = focal2fov(fy, cam._height);
            break;
        }
        default:
            throw std::runtime_error("Unsupported camera model");
        }

        cam._K = K;
    }

    float scene_scale = 1.0f;
    if (!images.empty()) {
        torch::Tensor scene_center = camera_locations.mean(0);                    // [3]
        torch::Tensor dists = torch::norm(camera_locations - scene_center, 2, 1); // [N]
        scene_scale = dists.max().item<float>() * 1.1f;
    }

    std::cout << "Training with " << out.size() << " images \n";
    std::cout << "Scene scale: " << scene_scale << "\n";
    return {std::move(out), scene_scale}; // +10% for safety
}

// -----------------------------------------------------------------------------
//  Public API functions
// -----------------------------------------------------------------------------
PointCloud read_colmap_point_cloud(const std::filesystem::path& filepath) {
    return read_point3D_binary(filepath / "sparse/0/points3D.bin");
}

std::tuple<std::vector<CameraData>, float> read_colmap_cameras_and_images(
    const std::filesystem::path& base,
    const std::string& images_folder,
    const int resolution) {

    // Extract downscaling factor from folder name
    float factor = get_downscale_factor(images_folder);

    std::cout << "Reading COLMAP data with downscale factor: " << factor << std::endl;

    auto cams = read_cameras_binary(base / "sparse/0/cameras.bin", factor);
    auto images = read_images_binary(base / "sparse/0/images.bin");

    return read_colmap_cameras(base, cams, images, images_folder, resolution);
}