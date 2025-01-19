#include "HelloEndpoint.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <memory>
#include <mutex>

// Convert build number to string
#define STRINGIZE(x) #x
#define STRINGIZE_VALUE_OF(x) STRINGIZE(x)

// This string will change with each build, using CMake-provided values
#define BUILD_INFO_STR "Build #" STRINGIZE_VALUE_OF(BUILD_NUMBER) " - " BUILD_TIMESTAMP

namespace plugins {
namespace endpoint {

// Thread-safe singleton for build info
class BuildInfo {
public:
    static BuildInfo& instance() {
        static BuildInfo instance;
        return instance;
    }

    std::string get() const {
        std::lock_guard<std::mutex> lock(mutex);
        // Always return the current build info
        return BUILD_INFO_STR;
    }

private:
    BuildInfo() {}
    mutable std::mutex mutex;
};

EndpointPlugin::Handler HelloEndpoint::createHandler() const {
    return [](const Request& req) -> Response {
        Response res{http::status::ok, req.version()};
        res.set(http::field::server, "Beast");
        res.set(http::field::content_type, "text/plain");
        res.keep_alive(req.keep_alive());

        // Get current time
        auto now = std::chrono::system_clock::now();
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        
        std::stringstream ss;
        ss << "Hello! HOTHOTYOYOYOOY Reload Test\n"
           << BuildInfo::instance().get() << "\n"
           << "Current time: " << std::put_time(std::localtime(&now_time_t), "%Y-%m-%d %H:%M:%S") << "\n";
        
        res.body() = ss.str();
        res.prepare_payload();
        return res;
    };
}

} // namespace endpoint
} // namespace plugins

// Export the plugin
EXPORT_PLUGIN(plugins::endpoint::HelloEndpoint) 