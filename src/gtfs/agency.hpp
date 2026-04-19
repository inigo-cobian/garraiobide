#pragma once
#include <string>

namespace gtfs {
    class Agency {
    private:
        std::string id;
        std::string name;

    public:
        [[nodiscard]] explicit Agency(std::string id, std::string name);

        [[nodiscard]] const std::string &get_id() const;

        [[nodiscard]] const std::string &get_name() const;
    };
}
