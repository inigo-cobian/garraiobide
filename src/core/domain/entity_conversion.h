#pragma once

#include <vector>

#include "agency.h"
#include "entrance.h"
#include "geo_feature.h"
#include "layer.h"
#include "route.h"
#include "stop.h"

namespace garraiobide::core::domain {

/// Convert a Stop to a GeoFeature with properties mirroring the GTFS-derived format.
[[nodiscard]] GeoFeature stop_to_feature(const Stop& stop);

/// Convert a Route to a GeoFeature with line geometry and route properties.
[[nodiscard]] GeoFeature route_to_feature(const Route& route);

/// Convert an Entrance to a GeoFeature with point geometry.
[[nodiscard]] GeoFeature entrance_to_feature(const Entrance& entrance);

/// Build a stops Layer from a collection of typed Stop entities.
[[nodiscard]] Layer stops_to_layer(const std::vector<Stop>& stops,
                                   const std::string& layer_name);

/// Build a routes Layer from a collection of typed Route entities.
[[nodiscard]] Layer routes_to_layer(const std::vector<Route>& routes,
                                    const std::string& layer_name);

/// Build an entrances Layer from a collection of typed Entrance entities.
[[nodiscard]] Layer entrances_to_layer(const std::vector<Entrance>& entrances,
                                       const std::string& layer_name);

}  // namespace garraiobide::core::domain
