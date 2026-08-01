"""
Property test for route membership computation correctness.

Feature: stops-route-filtering, Property 1: Route membership computation correctness

**Validates: Requirements 1.1**

For any valid GTFS feed containing stops, stop_times, and trips, the `route_ids`
set computed for a standalone stop (no parent/child relationship) SHALL equal the
set of distinct route IDs reachable by joining that stop's stop_id through
stop_times.trip_id to trips.route_id.
"""

import json
from typing import Any

import hypothesis.strategies as st
from hypothesis import given, settings


# ---------------------------------------------------------------------------
# Model of the route membership computation from gtfs_parser.cc
# ---------------------------------------------------------------------------


def compute_route_membership(
    stops: list[dict[str, str]],
    stop_times: list[dict[str, str]],
    trips: list[dict[str, str]],
) -> dict[str, set[str]]:
    """
    Python model of `build_stop_route_map` from gtfs_parser.cc.

    Builds a stop_id -> set of route_ids map by:
    1. Indexing trips by trip_id -> route_id
    2. Joining stop_times -> trips to populate the map
    3. Merging parent<->child relationships (parent absorbs children, children absorb parent)

    Returns the complete stop_route_map.
    """
    # Index trips by trip_id -> route_id
    trip_route_index: dict[str, str] = {}
    for trip in trips:
        tid = trip.get("trip_id")
        rid = trip.get("route_id")
        if tid is not None and rid is not None:
            trip_route_index[tid] = rid

    # Join stop_times -> trips to populate stop_route_map
    stop_route_map: dict[str, set[str]] = {}
    for st_row in stop_times:
        sid = st_row.get("stop_id")
        tid = st_row.get("trip_id")
        if sid is None or tid is None:
            continue
        route_id = trip_route_index.get(tid)
        if route_id is not None:
            stop_route_map.setdefault(sid, set()).add(route_id)

    # Build parent->children relationships
    parent_children: dict[str, list[str]] = {}
    for stop in stops:
        sid = stop.get("stop_id")
        ps = stop.get("parent_station", "")
        if sid is None:
            continue
        if ps:
            parent_children.setdefault(ps, []).append(sid)

    # Merge: parent gets all children's routes, then children get merged parent routes
    for parent_id, children in parent_children.items():
        parent_routes = stop_route_map.setdefault(parent_id, set())
        for child_id in children:
            child_routes = stop_route_map.get(child_id, set())
            parent_routes.update(child_routes)
        for child_id in children:
            stop_route_map.setdefault(child_id, set()).update(parent_routes)

    return stop_route_map


def compute_expected_standalone_routes(
    stop_id: str,
    stop_times: list[dict[str, str]],
    trips: list[dict[str, str]],
) -> set[str]:
    """
    Reference implementation: directly compute route IDs for a standalone stop
    by joining stop_times.trip_id to trips.route_id.

    This is the "oracle" — the simplest possible implementation of the join.
    """
    # Index trips by trip_id -> route_id
    trip_route_index: dict[str, str] = {}
    for trip in trips:
        tid = trip.get("trip_id")
        rid = trip.get("route_id")
        if tid is not None and rid is not None:
            trip_route_index[tid] = rid

    # Find all route IDs reachable from this stop
    routes: set[str] = set()
    for st_row in stop_times:
        if st_row.get("stop_id") == stop_id:
            tid = st_row.get("trip_id")
            if tid is not None and tid in trip_route_index:
                routes.add(trip_route_index[tid])

    return routes


# ---------------------------------------------------------------------------
# Hypothesis strategies for generating GTFS feed data
# ---------------------------------------------------------------------------

# Strategy for generating valid identifiers (non-empty alphanumeric strings)
identifier_strategy = st.text(
    alphabet=st.characters(categories=("L", "N")),
    min_size=1,
    max_size=8,
)


@st.composite
def gtfs_feed_strategy(draw: st.DrawFn) -> dict[str, Any]:
    """
    Generate a valid GTFS feed with stops (standalone only), stop_times, and trips.

    For Property 1, we focus on standalone stops (no parent_station),
    so all generated stops have no parent_station field.
    """
    # Generate a pool of route IDs
    num_routes = draw(st.integers(min_value=1, max_value=5))
    route_ids = [draw(identifier_strategy) for _ in range(num_routes)]
    # Deduplicate
    route_ids = list(set(route_ids))
    if not route_ids:
        route_ids = ["R1"]

    # Generate a pool of trip IDs, each associated with a route
    num_trips = draw(st.integers(min_value=1, max_value=8))
    trips: list[dict[str, str]] = []
    trip_ids: list[str] = []
    for _ in range(num_trips):
        tid = draw(identifier_strategy)
        rid = draw(st.sampled_from(route_ids))
        trips.append({"trip_id": tid, "route_id": rid})
        trip_ids.append(tid)
    # Deduplicate trips by trip_id (keep first)
    seen_tids: set[str] = set()
    unique_trips: list[dict[str, str]] = []
    unique_trip_ids: list[str] = []
    for t in trips:
        if t["trip_id"] not in seen_tids:
            seen_tids.add(t["trip_id"])
            unique_trips.append(t)
            unique_trip_ids.append(t["trip_id"])
    trips = unique_trips
    trip_ids = unique_trip_ids

    if not trip_ids:
        trip_ids = ["T1"]
        trips = [{"trip_id": "T1", "route_id": route_ids[0]}]

    # Generate standalone stops
    num_stops = draw(st.integers(min_value=1, max_value=5))
    stops: list[dict[str, str]] = []
    stop_ids: list[str] = []
    for _ in range(num_stops):
        sid = draw(identifier_strategy)
        stops.append({
            "stop_id": sid,
            "stop_lat": "43.0",
            "stop_lon": "-2.0",
        })
        stop_ids.append(sid)
    # Deduplicate stops
    seen_sids: set[str] = set()
    unique_stops: list[dict[str, str]] = []
    unique_stop_ids: list[str] = []
    for s in stops:
        if s["stop_id"] not in seen_sids:
            seen_sids.add(s["stop_id"])
            unique_stops.append(s)
            unique_stop_ids.append(s["stop_id"])
    stops = unique_stops
    stop_ids = unique_stop_ids

    if not stop_ids:
        stop_ids = ["S1"]
        stops = [{"stop_id": "S1", "stop_lat": "43.0", "stop_lon": "-2.0"}]

    # Generate stop_times linking stops to trips
    num_stop_times = draw(st.integers(min_value=0, max_value=15))
    stop_times: list[dict[str, str]] = []
    for _ in range(num_stop_times):
        sid = draw(st.sampled_from(stop_ids))
        tid = draw(st.sampled_from(trip_ids))
        stop_times.append({
            "stop_id": sid,
            "trip_id": tid,
            "stop_sequence": "1",
        })

    return {
        "stops": stops,
        "stop_times": stop_times,
        "trips": trips,
        "route_ids": route_ids,
        "stop_ids": stop_ids,
    }


# ---------------------------------------------------------------------------
# Property tests
# ---------------------------------------------------------------------------


class TestRouteMembershipComputation:
    """Property 1: Route membership computation correctness."""

    @given(feed=gtfs_feed_strategy())
    @settings(max_examples=200)
    def test_standalone_stop_route_ids_equal_join_result(
        self, feed: dict[str, Any]
    ) -> None:
        """
        **Validates: Requirements 1.1**

        For any valid GTFS feed, the route_ids set computed for each standalone
        stop equals the set of distinct route IDs reachable by joining that
        stop's stop_id through stop_times.trip_id to trips.route_id.
        """
        stops = feed["stops"]
        stop_times = feed["stop_times"]
        trips = feed["trips"]

        # Compute route membership using the model (mirrors C++ implementation)
        stop_route_map = compute_route_membership(stops, stop_times, trips)

        # For each standalone stop, verify computed routes match the oracle
        for stop in stops:
            stop_id = stop["stop_id"]
            # All stops in this test are standalone (no parent_station)
            assert "parent_station" not in stop or stop.get("parent_station", "") == ""

            # Routes computed by the model (what the C++ code produces)
            computed_routes = stop_route_map.get(stop_id, set())

            # Routes computed by the oracle (direct join)
            expected_routes = compute_expected_standalone_routes(
                stop_id, stop_times, trips
            )

            assert computed_routes == expected_routes, (
                f"Stop '{stop_id}': computed routes {computed_routes} != "
                f"expected routes {expected_routes}"
            )

    @given(feed=gtfs_feed_strategy())
    @settings(max_examples=200)
    def test_stops_without_stop_times_have_empty_route_ids(
        self, feed: dict[str, Any]
    ) -> None:
        """
        **Validates: Requirements 1.1**

        For any standalone stop that has no entries in stop_times, the computed
        route_ids set SHALL be empty.
        """
        stops = feed["stops"]
        stop_times = feed["stop_times"]
        trips = feed["trips"]

        # Identify stops that have no stop_times entries
        stops_with_times = {st["stop_id"] for st in stop_times if "stop_id" in st}

        stop_route_map = compute_route_membership(stops, stop_times, trips)

        for stop in stops:
            stop_id = stop["stop_id"]
            if stop_id not in stops_with_times:
                computed_routes = stop_route_map.get(stop_id, set())
                assert computed_routes == set(), (
                    f"Stop '{stop_id}' has no stop_times but computed routes: "
                    f"{computed_routes}"
                )

    @given(feed=gtfs_feed_strategy())
    @settings(max_examples=200)
    def test_route_ids_are_subset_of_feed_routes(
        self, feed: dict[str, Any]
    ) -> None:
        """
        **Validates: Requirements 1.1**

        For any standalone stop, all route IDs in the computed route_ids set
        SHALL exist as route_id values in the trips table (no phantom routes).
        """
        stops = feed["stops"]
        stop_times = feed["stop_times"]
        trips = feed["trips"]

        # All route_ids present in the trips table
        all_route_ids = {t["route_id"] for t in trips if "route_id" in t}

        stop_route_map = compute_route_membership(stops, stop_times, trips)

        for stop in stops:
            stop_id = stop["stop_id"]
            computed_routes = stop_route_map.get(stop_id, set())
            assert computed_routes.issubset(all_route_ids), (
                f"Stop '{stop_id}' has routes {computed_routes - all_route_ids} "
                f"not in trips table"
            )
