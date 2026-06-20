#pragma once
#include <cstdint>
#include <string>
#include <format>

namespace ui {
    using Color = std::uint_least32_t;

    std::string toHex(Color value);

}
