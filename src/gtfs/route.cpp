#include "route.hpp"

namespace gtfs {
Route::Route(const std::string& id, const std::string& name, const std::string& type, const std::string& color, const std::string& textColor)
    : id(id), name(name), type(type), color(color), textColor(textColor) {}

[[nodiscard]] const std::string&  Route::get_id() const {
  return id;
}

[[nodiscard]] const std::string&  Route::get_name() const {
  return name;
}

[[nodiscard]] const std::string&  Route::get_type() const {
  return type;
}

[[nodiscard]] const std::string&  Route::get_color() const {
  return color;
}

[[nodiscard]] const std::string&  Route::get_text_color() const {
  return textColor;
}
}