#include "agency.hpp"

namespace gtfs {

Agency::Agency(std::string id, std::string name) : id(id), name(name) {}

[[nodiscard]] std::string Agency::get_id() const {
  return id;
}

[[nodiscard]] std::string Agency::get_name() const {
  return name;
}

}