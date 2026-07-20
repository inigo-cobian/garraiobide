#pragma once

namespace garraiobide::core::domain {

/// A geographic coordinate (WGS84).
struct Coordinate {
    double latitude{0.0};
    double longitude{0.0};

    auto operator<=>(const Coordinate&) const = default;
};

}  // namespace garraiobide::core::domain
