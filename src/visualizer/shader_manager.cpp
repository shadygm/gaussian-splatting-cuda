#include "visualizer/shader_manager.hpp"
#include "visualizer/shader.hpp"
#include <iostream>
#include <stdexcept>

namespace gs {

    ShaderManager::ShaderManager(const std::filesystem::path& base_path)
        : shader_base_path_(base_path) {
        if (!std::filesystem::exists(shader_base_path_)) {
            throw std::runtime_error("Shader base path does not exist: " + shader_base_path_.string());
        }
    }

    std::shared_ptr<Shader> ShaderManager::loadShader(
        const std::string& name,
        const std::string& vert_file,
        const std::string& frag_file,
        bool create_buffer) {

        // Check if already loaded
        auto it = shaders_.find(name);
        if (it != shaders_.end()) {
            return it->second;
        }

        // Construct full paths
        auto vert_path = shader_base_path_ / vert_file;
        auto frag_path = shader_base_path_ / frag_file;

        // Verify files exist
        if (!std::filesystem::exists(vert_path)) {
            throw std::runtime_error("Vertex shader not found: " + vert_path.string());
        }
        if (!std::filesystem::exists(frag_path)) {
            throw std::runtime_error("Fragment shader not found: " + frag_path.string());
        }

        try {
            // Create shader
            auto shader = std::make_shared<Shader>(
                vert_path.string().c_str(),
                frag_path.string().c_str(),
                create_buffer);

            // Cache shader and info
            shaders_[name] = shader;
            shader_info_[name] = {vert_file, frag_file, "", create_buffer};

            std::cout << "Loaded shader '" << name << "' successfully" << std::endl;
            return shader;

        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to load shader '" + name + "': " + e.what());
        }
    }

    std::shared_ptr<Shader> ShaderManager::loadShaderWithGeometry(
        const std::string& name,
        const std::string& vert_file,
        const std::string& frag_file,
        const std::string& geom_file,
        bool create_buffer) {

        // For now, we'll throw an error since the Shader class doesn't support geometry shaders
        // This can be implemented later when Shader class is extended
        throw std::runtime_error("Geometry shaders not yet supported by Shader class");
    }

    std::shared_ptr<Shader> ShaderManager::getShader(const std::string& name) const {
        auto it = shaders_.find(name);
        if (it == shaders_.end()) {
            throw std::runtime_error("Shader '" + name + "' not found. Load it first with loadShader()");
        }
        return it->second;
    }

    bool ShaderManager::hasShader(const std::string& name) const {
        return shaders_.find(name) != shaders_.end();
    }

    void ShaderManager::reloadShader(const std::string& name) {
        auto info_it = shader_info_.find(name);
        if (info_it == shader_info_.end()) {
            throw std::runtime_error("Cannot reload shader '" + name + "': shader info not found");
        }

        const auto& info = info_it->second;

        // Remove old shader
        shaders_.erase(name);

        // Reload
        if (info.geom_file.empty()) {
            loadShader(name, info.vert_file, info.frag_file, info.create_buffer);
        } else {
            loadShaderWithGeometry(name, info.vert_file, info.frag_file, info.geom_file, info.create_buffer);
        }

        std::cout << "Reloaded shader '" << name << "'" << std::endl;
    }

    void ShaderManager::reloadAllShaders() {
        std::cout << "Reloading all shaders..." << std::endl;

        // Get list of shader names (can't iterate and modify)
        std::vector<std::string> shader_names;
        for (const auto& [name, _] : shader_info_) {
            shader_names.push_back(name);
        }

        // Reload each shader
        for (const auto& name : shader_names) {
            try {
                reloadShader(name);
            } catch (const std::exception& e) {
                std::cerr << "Failed to reload shader '" << name << "': " << e.what() << std::endl;
            }
        }

        std::cout << "Shader reload complete" << std::endl;
    }

} // namespace gs