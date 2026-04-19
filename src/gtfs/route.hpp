#pragma once

#include <string>

namespace gtfs {
    class Route {
    private:
        std::string id;
        std::string name;
        std::string type;
        std::string color;
        std::string textColor;

    public:
        explicit Route(std::string id, std::string name, std::string type, std::string color, std::string textColor);

        [[nodiscard]] const std::string &get_id() const;

        [[nodiscard]] const std::string &get_name() const;

        [[nodiscard]] const std::string &get_type() const;

        [[nodiscard]] const std::string &get_color() const;

        [[nodiscard]] const std::string &get_text_color() const;
    };
}
