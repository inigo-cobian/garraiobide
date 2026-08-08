#pragma once

#include <optional>
#include <string>

namespace garraiobide::core::domain {

/// A transit agency that operates routes.
struct Agency {
    std::string id;
    std::string name;
    std::optional<std::string> url;
    std::optional<std::string> timezone;
    std::optional<std::string> lang;
    std::optional<std::string> phone;
};

}  // namespace garraiobide::core::domain
