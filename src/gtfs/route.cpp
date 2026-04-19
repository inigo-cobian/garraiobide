#include "route.hpp"

#include <utility>

namespace gtfs {
    Route::Route(std::string id, std::string name, std::string type, std::string color, std::string textColor)
        : id(std::move(id)), name(std::move(name)), type(std::move(type)), color(std::move(color)),
          textColor(std::move(textColor)) {
    }

    const std::string &Route::get_id() const {
        return id;
    }

    const std::string &Route::get_name() const {
        return name;
    }

    const std::string &Route::get_type() const {
        return type;
    }

    const std::string &Route::get_color() const {
        return color;
    }

    const std::string &Route::get_text_color() const {
        return textColor;
    }
}
