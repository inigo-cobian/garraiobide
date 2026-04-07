#pragma once
#include <string>
#include <ogr_geometry.h>
#include <optional>

namespace gtfs {
    enum class LocationType : int {
        Stop = 0, // platform / stop
        Station = 1, // station complex
        Entrance = 2, // entrance / exit
        GenericNode = 3,
        BoardingArea = 4
    };

    class Stop {
        // https://gtfs.org/documentation/schedule/reference/#stopstxt
    private:
        std::string id;
        std::string name;
        OGRPoint point;
        LocationType locationType = LocationType::Stop;
        std::optional<std::string> parentStation;

    public:
        explicit Stop(std::string id, std::string name, float latitude, float longitude,
                      LocationType type = LocationType::Stop,
                      std::optional<std::string> parent = std::nullopt);

        explicit Stop(std::string id, std::string name, float latitude, float longitude, int type,
                      std::optional<std::string> parent = std::nullopt);

        explicit Stop(std::string id, std::string name, OGRPoint point, LocationType type = LocationType::Stop,
                      std::optional<std::string> parent = std::nullopt);

        explicit Stop(std::string id, std::string name, OGRPoint point, int type,
                      std::optional<std::string> parent = std::nullopt);


        [[nodiscard]] std::string get_id() const;

        [[nodiscard]] std::string get_name() const;

        [[nodiscard]] OGRPoint get_point() const;

        [[nodiscard]] LocationType location_type() const;

        [[nodiscard]] std::optional<std::string> parent_station() const;
    };
}
