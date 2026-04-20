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

    namespace shapes {
        static const std::string ID = "shape_id",
                LATITUDE = "shape_pt_lat",
                LONGITUDE = "shape_pt_lon",
                SEQUENCE = "shape_pt_sequence";
    }

    namespace trips {
        static const std::string ID = "trip_id",
                ROUTE_ID = "route_id",
                HEADSIGN = "trip_headsign",
                DIRECTION_ID = "direction_id",
                SHAPE_ID = "shape_id";
    }
}
