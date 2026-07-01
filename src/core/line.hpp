#pragma once
#include "ui/color.hpp"
#include <string>

namespace core {
    /**
     * @brief Represents a public transit line (route) with its visual attributes.
     *
     * A Line has a unique ID, a display name, a background color, text color
     * which should be the same as used by the real-life operator
     * and a source identifier indicating where the data originated.
     */
    class Line {
        std::string id;
        std::string name;
        ui::Color color;
        ui::Color textColor;
        std::string source;

    public:
        /**
         * @brief Construct a new Line.
         * @param id Unique identifier.
         * @param name Display name.
         * @param color Primary colour (e.g. for the line on a map).
         * @param text_color Contrast colour for text on the line.
         * @param source Data source identifier (e.g. "metro-bilbao").
         */
        [[nodiscard]] Line(const std::string &id, const std::string &name, ui::Color color,
                           ui::Color text_color, const std::string &source);

        [[nodiscard]] std::string get_id() const;

        [[nodiscard]] std::string get_name() const;

        [[nodiscard]] ui::Color get_color() const;

        [[nodiscard]] ui::Color get_text_color() const;

        [[nodiscard]] std::string get_source() const;
    };
}
