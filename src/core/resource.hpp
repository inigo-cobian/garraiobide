#pragma once

#include <string>
#include <chrono>
#include <nlohmann/json.hpp>
#include "resource_type.hpp"

using json = nlohmann::json;

namespace core {
    class Resource {
        std::string id;
        std::string url;
        std::string name;
        ResourceType type;
        std::chrono::time_point<std::chrono::high_resolution_clock> creation_time;
        std::chrono::time_point<std::chrono::high_resolution_clock> last_modified;

    public:
        Resource(std::string name, std::string url, ResourceType type);
        ~Resource();
        [[nodiscard]] const std::string& get_url() const;
        [[nodiscard]] const std::string& get_name() const;
        [[nodiscard]] ResourceType get_type() const;
        [[nodiscard]] std::chrono::time_point<std::chrono::high_resolution_clock> get_creation_time() const;
        [[nodiscard]] std::chrono::time_point<std::chrono::high_resolution_clock> get_last_modified() const;
        [[nodiscard]] json as_json() const;
    };
}
