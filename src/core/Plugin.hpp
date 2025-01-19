#pragma once

#include <string>
#include <memory>

namespace core {

enum class PluginType {
    CONTROLLER,
    ENDPOINT,
    ROUTER
};

class Plugin {
public:
    virtual ~Plugin() = default;
    virtual std::string getName() const = 0;
    virtual PluginType getType() const = 0;
    virtual void initialize() = 0;
    virtual void cleanup() = 0;
};

// Plugin creation function type
using CreatePluginFunc = std::shared_ptr<Plugin>(*)();

} // namespace core

// Macro to export plugin creation function
#define EXPORT_PLUGIN(PluginClass) \
    extern "C" std::shared_ptr<core::Plugin> createPlugin() { \
        return std::make_shared<PluginClass>(); \
    }
