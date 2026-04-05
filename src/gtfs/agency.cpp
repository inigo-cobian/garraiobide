#include "agency.hpp"

namespace gtfs {

Agency::Agency(std::string id, std::string name) : id(id), name(name) {}

[[no_discard]] std::string Agency::get_id() const {
  return id;
}

[[no_discard]] std::string Agency::get_name() const {
  return name;
}

}