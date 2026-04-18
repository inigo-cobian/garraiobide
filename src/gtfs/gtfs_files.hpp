#pragma once

#include <string>
#include <array>

namespace gtfs::files {
    static constexpr std::string_view AGENCY = "agency.txt",
                         ROUTES = "routes.txt",
                         TRIPS = "trips.txt",
                         STOPS = "stops.txt",
                         STOPS_TIMES = "stops_times.txt",
                         CALENDAR = "calendar.txt",
                         CALENDAR_DATES = "calendar_dates.txt",
                         FARE_ATTRIBUTES = "fare_attributes.txt",
                         FARE_RULES = "fare_rules.txt",
                         TIMEFRAMES = "timeframes.txt",
                         RIDER_CATEGORIES = "rider_categories.txt",
                         FARE_MEDIA = "fare_media.txt",
                         FARE_PRODUCTS = "fare_products.txt",
                         FARE_LEG_RULES = "fare_leg_rules.txt",
                         FARE_LEG_JOIN_RULES = "fare_leg_join_rules.txt",
                         FARE_TRANSFER_RULES = "fare_transfer_rules.txt",
                         AREAS = "areas.txt",
                         STOP_AREAS = "stop_areas.txt",
                         NETWORKS = "networks.txt",
                         ROUTE_NETWORKS = "route_networks.txt",
                         SHAPES = "shapes.txt",
                         FREQUENCIES = "frequencies.txt",
                         TRANSFERS = "transfers.txt",
                         PATHWAYS = "pathways.txt",
                         LEVELS = "levels.txt",
                         LOCATION_GROUPS = "location_groups.txt",
                         LOCATION_GROUP_STOPS = "location_group_stops.txt",
                         LOCATIONS = "locations.geojson",
                         BOOKING_RULES = "booking_rules.txt",
                         TRANSLATIONS = "translations.txt",
                         FEED_INFORMATION = "feed_info.txt",
                         ATTRIBUTIONS = "attributions.txt";

    constexpr std::array<std::string_view, 7> REQUIRED_FILES = {
        AGENCY, ROUTES, TRIPS, STOPS, STOPS_TIMES, CALENDAR, CALENDAR_DATES
    };

    constexpr bool is_required(std::string_view filename) noexcept {
        for (std::string_view req : REQUIRED_FILES) {
            if (filename == req) {
                return true;
            }
        }
        return false;
    }
}