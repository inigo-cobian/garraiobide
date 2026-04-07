#pragma once
#include <string>

namespace gtfs::fields {
    namespace stops {
        static const std::string ID = "stop_id",
                NAME = "stop_name",
                LONGITUDE = "stop_lon",
                LATITUDE = "stop_lat",
                TYPE = "location_type",
                PARENT = "parent_station";
    }

    namespace agency {
        static const std::string ID = "agency_id",
                NAME = "agency_name";
    }

    namespace routes {
        static const std::string ID = "route_id",
                SHORT_NAME = "route_short_name",
                LONG_NAME = "route_long_name",
                TYPE = "route_type",
                COLOR = "route_color",
                TEXT_COLOR = "route_text_color";
    }
}
