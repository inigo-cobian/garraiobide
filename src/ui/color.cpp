#include "color.hpp"

namespace ui {
    std::string toHex(Color value) {
        return std::format("{:X}", value);
    }
}