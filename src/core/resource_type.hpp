#pragma once

#include <string>

namespace core {
    enum class ResourceType {
        gtfs,
    };

    constexpr std::array<std::string_view, 1> resourceTypeNames = {
        "gtfs",
    };

    constexpr std::string_view toString(ResourceType type) {
        auto idx = std::to_underlying(type);
        if (idx >= resourceTypeNames.size())
            throw std::out_of_range("Invalid Resource Type value");
        return resourceTypeNames[idx];
    }
}