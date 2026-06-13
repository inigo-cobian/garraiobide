#pragma once
#include "ui/Color.hpp"
#include <string>

namespace core {
    class Line {
        std::string id;
        std::string name;
        ui::Color color;
        ui::Color textColor;
        std::string source;

    public:
        [[nodiscard]] Line(std::string id, std::string name, ui::Color color, ui::Color text_color,
                           std::string source);

        [[nodiscard]] std::string get_id() const;

        [[nodiscard]] std::string get_name() const;

        [[nodiscard]] ui::Color get_color() const;

        [[nodiscard]] ui::Color get_text_color() const;

        [[nodiscard]] std::string get_source() const;
    };
}
