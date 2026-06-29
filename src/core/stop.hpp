#pragma once
#include <string>

#include "gtfs/stop.hpp"

namespace core {
    class Stop {
        std::string id;
        std::string name;
        double longitude;
        double latitude;
        std::string source;

    public:
        Stop(const std::string &id, const std::string &name, double longitude, double latitude,
             const std::string &source);

        Stop(gtfs::Stop gtfs_stop, const std::string &source);

        Stop(Stop &&other) noexcept;

        [[nodiscard]] const std::string &get_id() const;

        [[nodiscard]] const std::string &get_name() const;

        [[nodiscard]] double get_longitude() const;

        [[nodiscard]] double get_latitude() const;

        [[nodiscard]] const std::string &get_source() const;

        Stop(const Stop &other) = default;

        ~Stop() = default;
    };
}
