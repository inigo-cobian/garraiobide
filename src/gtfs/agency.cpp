#include "agency.hpp"

#include <utility>

namespace gtfs {
    Agency::Agency(std::string id, std::string name) : id(std::move(id)), name(std::move(name)) {
    }

    [[nodiscard]] const std::string &Agency::get_id() const {
        return id;
    }

    [[nodiscard]] const std::string &Agency::get_name() const {
        return name;
    }
}
