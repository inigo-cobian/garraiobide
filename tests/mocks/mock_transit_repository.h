#pragma once

#include <algorithm>
#include <expected>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../src/core/ports/transit_repository_port.h"

namespace garraiobide::tests {

/// In-memory mock implementation of TransitRepositoryPort for unit testing.
class MockTransitRepository final : public core::ports::TransitRepositoryPort {
   public:
    using Error = core::ports::TransitRepositoryError;

    // ── Agency ────────────────────────────────────────────────────────

    std::expected<void, Error> save_agency(const core::domain::Agency& agency) override {
        std::lock_guard lock(mutex_);
        auto [_, inserted] = agencies_.try_emplace(agency.id, agency);
        if (!inserted) return std::unexpected(Error::DuplicateEntity);
        return {};
    }

    std::expected<core::domain::Agency, Error> find_agency(const std::string& id) override {
        std::lock_guard lock(mutex_);
        auto it = agencies_.find(id);
        if (it == agencies_.end()) return std::unexpected(Error::NotFound);
        return it->second;
    }

    std::expected<std::vector<core::domain::Agency>, Error> list_agencies() override {
        std::lock_guard lock(mutex_);
        std::vector<core::domain::Agency> result;
        result.reserve(agencies_.size());
        for (const auto& [_, a] : agencies_) result.push_back(a);
        return result;
    }

    std::expected<void, Error> remove_agency(const std::string& id) override {
        std::lock_guard lock(mutex_);
        if (agencies_.erase(id) == 0) return std::unexpected(Error::NotFound);
        // Cascade: remove routes belonging to this agency
        std::erase_if(routes_, [&](const auto& pair) {
            return pair.second.agency_id == id;
        });
        return {};
    }

    // ── Route ─────────────────────────────────────────────────────────

    std::expected<void, Error> save_route(const core::domain::Route& route) override {
        std::lock_guard lock(mutex_);
        if (!agencies_.contains(route.agency_id))
            return std::unexpected(Error::ForeignKeyViolation);
        auto [_, inserted] = routes_.try_emplace(route.id, route);
        if (!inserted) return std::unexpected(Error::DuplicateEntity);
        return {};
    }

    std::expected<core::domain::Route, Error> find_route(const std::string& id) override {
        std::lock_guard lock(mutex_);
        auto it = routes_.find(id);
        if (it == routes_.end()) return std::unexpected(Error::NotFound);
        return it->second;
    }

    std::expected<std::vector<core::domain::Route>, Error>
    list_routes(const std::string& agency_id) override {
        std::lock_guard lock(mutex_);
        std::vector<core::domain::Route> result;
        for (const auto& [_, r] : routes_) {
            if (agency_id.empty() || r.agency_id == agency_id)
                result.push_back(r);
        }
        return result;
    }

    std::expected<void, Error> remove_route(const std::string& id) override {
        std::lock_guard lock(mutex_);
        if (routes_.erase(id) == 0) return std::unexpected(Error::NotFound);
        // Cascade: remove route_stops entries
        std::erase_if(route_stops_, [&](const auto& rs) {
            return rs.route_id == id;
        });
        return {};
    }

    // ── Stop ──────────────────────────────────────────────────────────

    std::expected<void, Error> save_stop(const core::domain::Stop& stop) override {
        std::lock_guard lock(mutex_);
        auto [_, inserted] = stops_.try_emplace(stop.id, stop);
        if (!inserted) return std::unexpected(Error::DuplicateEntity);
        return {};
    }

    std::expected<core::domain::Stop, Error> find_stop(const std::string& id) override {
        std::lock_guard lock(mutex_);
        auto it = stops_.find(id);
        if (it == stops_.end()) return std::unexpected(Error::NotFound);
        return it->second;
    }

    std::expected<std::vector<core::domain::Stop>, Error>
    list_stops(const std::string& stop_type_filter) override {
        std::lock_guard lock(mutex_);
        std::vector<core::domain::Stop> result;
        for (const auto& [_, s] : stops_) {
            if (stop_type_filter.empty()) {
                result.push_back(s);
            } else if (stop_type_filter == "parent_station" &&
                       s.stop_type == core::domain::StopType::ParentStation) {
                result.push_back(s);
            } else if (stop_type_filter == "child_stop" &&
                       s.stop_type == core::domain::StopType::ChildStop) {
                result.push_back(s);
            } else if (stop_type_filter == "standalone" &&
                       s.stop_type == core::domain::StopType::Standalone) {
                result.push_back(s);
            }
        }
        return result;
    }

    std::expected<void, Error> remove_stop(const std::string& id) override {
        std::lock_guard lock(mutex_);
        if (stops_.erase(id) == 0) return std::unexpected(Error::NotFound);
        // Cascade: remove entrances and route_stops
        std::erase_if(entrances_, [&](const auto& pair) {
            return pair.second.stop_id == id;
        });
        std::erase_if(route_stops_, [&](const auto& rs) {
            return rs.stop_id == id;
        });
        return {};
    }

    std::expected<std::vector<core::domain::Stop>, Error>
    find_children_of(const std::string& parent_stop_id) override {
        std::lock_guard lock(mutex_);
        std::vector<core::domain::Stop> result;
        for (const auto& [_, s] : stops_) {
            if (s.parent_stop_id.has_value() && s.parent_stop_id.value() == parent_stop_id) {
                result.push_back(s);
            }
        }
        return result;
    }

    std::expected<std::vector<core::domain::Stop>, Error>
    query_stops(const core::domain::BoundingBox& extent) override {
        std::lock_guard lock(mutex_);
        std::vector<core::domain::Stop> result;
        for (const auto& [_, s] : stops_) {
            if (extent.contains(s.position)) {
                result.push_back(s);
            }
        }
        return result;
    }

    // ── Entrance ──────────────────────────────────────────────────────

    std::expected<void, Error> save_entrance(const core::domain::Entrance& entrance) override {
        std::lock_guard lock(mutex_);
        if (!stops_.contains(entrance.stop_id))
            return std::unexpected(Error::ForeignKeyViolation);
        auto [_, inserted] = entrances_.try_emplace(entrance.id, entrance);
        if (!inserted) return std::unexpected(Error::DuplicateEntity);
        return {};
    }

    std::expected<core::domain::Entrance, Error>
    find_entrance(const std::string& id) override {
        std::lock_guard lock(mutex_);
        auto it = entrances_.find(id);
        if (it == entrances_.end()) return std::unexpected(Error::NotFound);
        return it->second;
    }

    std::expected<std::vector<core::domain::Entrance>, Error>
    list_entrances(const std::string& stop_id) override {
        std::lock_guard lock(mutex_);
        std::vector<core::domain::Entrance> result;
        for (const auto& [_, e] : entrances_) {
            if (e.stop_id == stop_id) result.push_back(e);
        }
        return result;
    }

    std::expected<void, Error> remove_entrance(const std::string& id) override {
        std::lock_guard lock(mutex_);
        if (entrances_.erase(id) == 0) return std::unexpected(Error::NotFound);
        return {};
    }

    // ── Route-Stop relationships ──────────────────────────────────────

    std::expected<void, Error>
    add_route_stop(const std::string& route_id, const std::string& stop_id,
                   int stop_sequence) override {
        std::lock_guard lock(mutex_);
        if (!routes_.contains(route_id) || !stops_.contains(stop_id))
            return std::unexpected(Error::ForeignKeyViolation);
        route_stops_.push_back({route_id, stop_id, stop_sequence});
        return {};
    }

    std::expected<void, Error>
    remove_route_stop(const std::string& route_id, const std::string& stop_id) override {
        std::lock_guard lock(mutex_);
        auto before = route_stops_.size();
        std::erase_if(route_stops_, [&](const auto& rs) {
            return rs.route_id == route_id && rs.stop_id == stop_id;
        });
        if (route_stops_.size() == before) return std::unexpected(Error::NotFound);
        return {};
    }

    std::expected<std::vector<core::domain::Stop>, Error>
    find_stops_for_route(const std::string& route_id) override {
        std::lock_guard lock(mutex_);
        // Collect and sort by sequence
        struct Entry {
            int seq;
            std::string stop_id;
        };
        std::vector<Entry> entries;
        for (const auto& rs : route_stops_) {
            if (rs.route_id == route_id) {
                entries.push_back({rs.stop_sequence, rs.stop_id});
            }
        }
        std::sort(entries.begin(), entries.end(),
                  [](const Entry& a, const Entry& b) { return a.seq < b.seq; });

        std::vector<core::domain::Stop> result;
        for (const auto& e : entries) {
            auto it = stops_.find(e.stop_id);
            if (it != stops_.end()) result.push_back(it->second);
        }
        return result;
    }

    std::expected<std::vector<core::domain::Route>, Error>
    find_routes_for_stop(const std::string& stop_id) override {
        std::lock_guard lock(mutex_);
        std::vector<core::domain::Route> result;
        for (const auto& rs : route_stops_) {
            if (rs.stop_id == stop_id) {
                auto it = routes_.find(rs.route_id);
                if (it != routes_.end()) result.push_back(it->second);
            }
        }
        return result;
    }

   private:
    struct RouteStop {
        std::string route_id;
        std::string stop_id;
        int stop_sequence;
    };

    std::mutex mutex_;
    std::unordered_map<std::string, core::domain::Agency> agencies_;
    std::unordered_map<std::string, core::domain::Route> routes_;
    std::unordered_map<std::string, core::domain::Stop> stops_;
    std::unordered_map<std::string, core::domain::Entrance> entrances_;
    std::vector<RouteStop> route_stops_;
};

}  // namespace garraiobide::tests
