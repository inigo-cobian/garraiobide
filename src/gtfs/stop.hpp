#pragma once
#include <string>
#include <ogr_geometry.h>

namespace gtfs {
    class Stop {
        // https://gtfs.org/documentation/schedule/reference/#stopstxt
    private:
        std::string id;
        std::string name;
        OGRPoint point;
        // TODO manage entrance/exits as a different thing
    public:
        explicit Stop(std::string id, std::string name, float latitude, float longitude);

        explicit Stop(std::string id, std::string name, OGRPoint point);

        [[nodiscard]] std::string get_id() const;

        [[nodiscard]] std::string get_name() const;

        [[nodiscard]] OGRPoint get_point() const;
    };
}
