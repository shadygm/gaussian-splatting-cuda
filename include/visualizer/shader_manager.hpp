#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

// Forward declaration
class Shader;

namespace gs {

    class ShaderManager {
    public:
        explicit ShaderManager(const std::filesystem::path& base_path);
        ~ShaderManager() = default;

        // Delete copy, allow move
        ShaderManager(const ShaderManager&) = delete;
        ShaderManager& operator=(const ShaderManager&) = delete;
        ShaderManager(ShaderManager&&) = default;
        ShaderManager& operator=(ShaderManager&&) = default;

        // Load shader with caching
        std::shared_ptr<Shader> loadShader(
            const std::string& name,
            const std::string& vert_file,
            const std::string& frag_file,
            bool create_buffer = true);

        // Load shader with geometry shader
        std::shared_ptr<Shader> loadShaderWithGeometry(
            const std::string& name,
            const std::string& vert_file,
            const std::string& frag_file,
            const std::string& geom_file,
            bool create_buffer = true);

        // Get cached shader
        std::shared_ptr<Shader> getShader(const std::string& name) const;

        // Check if shader exists
        bool hasShader(const std::string& name) const;

        // Reload specific shader
        void reloadShader(const std::string& name);

        // Reload all shaders (useful for development)
        void reloadAllShaders();

        // Get base path
        const std::filesystem::path& getBasePath() const { return shader_base_path_; }

    private:
        struct ShaderInfo {
            std::string vert_file;
            std::string frag_file;
            std::string geom_file; // Empty if no geometry shader
            bool create_buffer;
        };

        std::filesystem::path shader_base_path_;
        std::unordered_map<std::string, std::shared_ptr<Shader>> shaders_;
        std::unordered_map<std::string, ShaderInfo> shader_info_;
    };

} // namespace gs