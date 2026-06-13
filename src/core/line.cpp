//
// Created by cosmos on 6/14/26.
//

#include "line.hpp"

#include <utility>

namespace core {
    Line::Line(const std::string & id, const std::string &name, const ui::Color color, const ui::Color text_color,
               const std::string & source)
        : id(id),
          name(name),
          color(color),
          textColor(text_color),
          source(source) {
    }

    std::string Line::get_id() const {
        return id;
    }

    std::string Line::get_name() const {
        return name;
    }

    ui::Color Line::get_color() const {
        return color;
    }

    ui::Color Line::get_text_color() const {
        return textColor;
    }

    std::string Line::get_source() const {
        return source;
    }
}
