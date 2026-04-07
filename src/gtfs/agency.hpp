#pragma once
#include <string>

namespace gtfs {
    class Agency {
    private:
        std::string id;
        std::string name;

    public:
        explicit Agency(std::string id, std::string name);

        [[nodiscard]] std::string get_id() const;

        [[nodiscard]] std::string get_name() const;
    };
}
