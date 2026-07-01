#pragma once
#include <cstdint>
#include <string>
#include <format>

namespace ui {
    /**
     * @brief Type alias for a 24‑bit RGB colour stored as a 32‑bit integer
     */
    using Color = std::uint_least32_t;

    /**
     * @brief Convert a Color value to a hexadecimal string.
     * @param value The color value.
     * @return std::string A string like "#RRGGBB" (without alpha).
     */
    std::string toHex(Color value);
}
