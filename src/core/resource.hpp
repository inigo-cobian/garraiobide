#pragma once

#include <string>
#include <chrono>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace core {
    enum class Type {
        GTFS,
      };


    class Resource {
        std::string id;
        std::string url;
        std::string name;
        Type type;
        std::chrono::time_point<std::chrono::high_resolution_clock> creation_time;
        std::chrono::time_point<std::chrono::high_resolution_clock> last_modified;

    public:
        Resource(std::string name, std::string url, Type type);
        ~Resource();
        [[nodiscard]] const std::string& get_url() const;
        [[nodiscard]] const std::string& get_name() const;
        [[nodiscard]] Type get_type() const;
        [[nodiscard]] std::chrono::time_point<std::chrono::high_resolution_clock> get_creation_time() const;
        [[nodiscard]] std::chrono::time_point<std::chrono::high_resolution_clock> get_last_modified() const;
        [[nodiscard]] json as_json() const;
    };
}
