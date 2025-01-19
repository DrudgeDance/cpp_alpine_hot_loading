#pragma once

#include "Plugin.hpp"
#include <string>
#include <memory>
#include <unordered_map>
#include <filesystem>

namespace core {

class DynamicLoader {
public:
    DynamicLoader() = default;
    ~DynamicLoader();

    // Prevent copying
    DynamicLoader(const DynamicLoader&) = delete;
    DynamicLoader& operator=(const DynamicLoader&) = delete;

    // Load a plugin from a shared library
    std::shared_ptr<Plugin> loadPlugin(const std::filesystem::path& libraryPath);
    
    // Unload a plugin
    void unloadPlugin(const std::string& pluginName);
    
    // Get a loaded plugin by name
    std::shared_ptr<Plugin> getPlugin(const std::string& pluginName) const;

private:
    struct PluginInfo {
        void* handle;
        std::shared_ptr<Plugin> plugin;
    };
    
    std::unordered_map<std::string, PluginInfo> loadedPlugins;
};

} // namespace core
