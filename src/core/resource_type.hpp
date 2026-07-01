#pragma once

#include <string>
#include <array>
#include <stdexcept>

namespace core {
    /**
     * @brief Enumeration of supported resource types.
     */
    enum class ResourceType {
        gtfs, ///< General Transit Feed Specification (GTFS) data.
    };

    constexpr std::array<std::string_view, 1> resourceTypeNames = {
        "gtfs",
    };

    /**
     * @brief Convert a ResourceType to its string representation.
     * @param type The enumeration value.
     * @return std::string_view The name (e.g., "gtfs").
     * @throws std::out_of_range If the value is invalid (should not happen with the enum).
     */
    constexpr std::string_view toString(const ResourceType type) {
        const auto idx = std::to_underlying(type);
        if (idx >= resourceTypeNames.size())
            throw std::out_of_range("Invalid Resource Type value");
        return resourceTypeNames[idx];
    }
}
