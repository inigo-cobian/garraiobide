#pragma once

#include <string>
#include <unordered_map>
#include <variant>

namespace garraiobide::core::domain {

/// Property value types supported in feature attributes.
using PropertyValue = std::variant<std::string, double, int64_t, bool>;

/// Key-value attribute bag attached to a GeoFeature.
using Properties = std::unordered_map<std::string, PropertyValue>;

}  // namespace garraiobide::core::domain
