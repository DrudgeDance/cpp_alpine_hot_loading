#pragma once

#include "EndpointPlugin.hpp"

namespace plugins {
namespace endpoint {

class HelloEndpoint : public EndpointPlugin {
public:
    std::string getName() const override { return "HelloEndpoint"; }
    void initialize() override {}
    
    std::string getPath() const override { return "/hello"; }
    std::string getMethod() const override { return "GET"; }

protected:
    Handler createHandler() const override;
};

} // namespace endpoint
} // namespace plugins 