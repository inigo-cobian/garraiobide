#include "resource.hpp"

#include <utility>

core::Resource::Resource(std::string name, std::string url, const ResourceType type) :
    name(std::move(name)), url(std::move(url)), type(type) {
    this->creation_time = this->last_modified = std::chrono::high_resolution_clock::now();
}

core::Resource::~Resource() = default;

const std::string & core::Resource::get_url() const {
    return url;
}

const std::string & core::Resource::get_name() const {
    return name;
}

core::ResourceType core::Resource::get_type() const {
    return type;
}

std::chrono::time_point<std::chrono::high_resolution_clock> core::Resource::get_creation_time() const {
    return creation_time;
}

std::chrono::time_point<std::chrono::high_resolution_clock> core::Resource::get_last_modified() const {
    return last_modified;
}

json core::Resource::as_json() const {
    json j;
    j["name"] = name;
    j["type"] = toString(type);
    j["last_modified"] = last_modified.time_since_epoch().count();
    j["creation_time"] = creation_time.time_since_epoch().count();
    return j;
}
