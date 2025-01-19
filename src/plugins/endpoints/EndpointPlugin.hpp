#pragma once

#include "../../core/Plugin.hpp"
#include <boost/beast/http.hpp>
#include <string>
#include <functional>

namespace http = boost::beast::http;

namespace plugins {
namespace endpoint {

class EndpointPlugin : public core::Plugin {
public:
    using Request = http::request<http::string_body>;
    using Response = http::response<http::string_body>;
    using Handler = std::function<Response(const Request&)>;

    virtual ~EndpointPlugin() = default;
    
    // Implement Plugin interface
    core::PluginType getType() const override { return core::PluginType::ENDPOINT; }
    void cleanup() override { handler_ = nullptr; }  // Clear the cached handler
    
    // Endpoint-specific interface
    virtual std::string getPath() const = 0;
    virtual std::string getMethod() const = 0;
    
    // Get the handler, creating it if necessary
    Handler getHandler() const {
        if (!handler_) {
            handler_ = createHandler();
        }
        return handler_;
    }

protected:
    // Create a new handler instance
    virtual Handler createHandler() const = 0;

private:
    mutable Handler handler_;  // Cache the handler
};

} // namespace endpoint
} // namespace plugins 