#include "DynamicLoader.hpp"
#include <dlfcn.h>
#include <stdexcept>
#include <iostream>
#include <filesystem>
#include <thread>
#include <chrono>

namespace core {

DynamicLoader::~DynamicLoader() {
    for (const auto& [name, info] : loadedPlugins) {
        if (info.handle) {
            std::cout << "Closing plugin: " << name << std::endl;
            if (info.plugin) {
                info.plugin->cleanup();
            }
            dlclose(info.handle);
        }
    }
}

std::shared_ptr<Plugin> DynamicLoader::loadPlugin(const std::filesystem::path& path) {
    auto abs_path = std::filesystem::absolute(path).string();
    
    // Check if plugin is already loaded
    auto it = loadedPlugins.find(abs_path);
    if (it != loadedPlugins.end()) {
        return it->second.plugin;
    }

    // Load the shared library
    void* handle = dlopen(abs_path.c_str(), RTLD_NOW);
    if (!handle) {
        throw std::runtime_error("Failed to load library: " + std::string(dlerror()));
    }

    // Get the create function
    auto create = reinterpret_cast<CreatePluginFunc>(dlsym(handle, "createPlugin"));
    if (!create) {
        dlclose(handle);
        throw std::runtime_error("Failed to get createPlugin function");
    }

    // Create the plugin
    auto plugin = create();
    if (!plugin) {
        dlclose(handle);
        throw std::runtime_error("Failed to create plugin");
    }
    
    // Store the plugin info
    PluginInfo info;
    info.handle = handle;
    info.plugin = plugin;
    loadedPlugins[abs_path] = std::move(info);
    
    return plugin;
}

void DynamicLoader::unloadPlugin(const std::string& path) {
    auto it = loadedPlugins.find(path);
    if (it != loadedPlugins.end()) {
        // Close the library handle
        dlclose(it->second.handle);
        // Remove from map
        loadedPlugins.erase(it);
    }
}

std::shared_ptr<Plugin> DynamicLoader::getPlugin(const std::string& pluginPath) const {
    // Convert to absolute path if it's not already
    auto abs_path = std::filesystem::absolute(pluginPath).string();
    auto it = loadedPlugins.find(abs_path);
    if (it != loadedPlugins.end()) {
        return it->second.plugin;
    }
    return nullptr;
}

} // namespace core
