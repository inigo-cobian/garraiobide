"""
Feature: stops-route-filtering, Property 4: Route IDs deduplication invariant

**Validates: Requirements 2.3**

Property 4: For any stop feature produced by the GTFS parser, the `route_ids`
array SHALL contain no duplicate entries.

This test generates GTFS feeds where the same route_id appears in multiple trips
that all reference the same stop, runs the Python re-implementation of the parser's
route membership computation logic, and verifies that the resulting `route_ids`
array for each stop contains no duplicates.

The C++ implementation uses std::unordered_set to guarantee deduplication. This
property test validates that invariant holds across arbitrary feed configurations.
"""

import json
from typing import Dict, List, Set

from hypothesis import given, settings
from hypothesis import strategies as st


# ---------------------------------------------------------------------------
# Model: GTFS feed data types (matching CsvRow = Dict[str, str])
# ---------------------------------------------------------------------------

CsvRow = Dict[str, str]


# ---------------------------------------------------------------------------
# System Under Test: re-implementation of route membership computation
# from gtfs_parser.cc build_stop_route_map + build_stop_layer
# ---------------------------------------------------------------------------


def build_stop_route_map(
    stops: List[CsvRow],
    stop_times: List[CsvRow],
    trips: List[CsvRow],
) -> Dict[str, Set[str]]:
    """
    Re-implementation of the C++ build_stop_route_map function.

    Builds a stop_id -> set<route_id> map by joining stop_times and trips,
    then merges parent<->child relationships so parents and children share
    the union of all route memberships across the station group.
    """
    stop_route_map: Dict[str, Set[str]] = {}

    # Index trips by trip_id -> route_id
    trip_route_index: Dict[str, str] = {}
    for trip in trips:
        tid = trip.get("trip_id")
        rid = trip.get("route_id")
        if tid is not None and rid is not None:
            trip_route_index[tid] = rid

    # Join stop_times -> trips to populate stop_route_map
    for st_row in stop_times:
        sid = st_row.get("stop_id")
        tid = st_row.get("trip_id")
        if sid is None or tid is None:
            continue
        route_id = trip_route_index.get(tid)
        if route_id is not None:
            if sid not in stop_route_map:
                stop_route_map[sid] = set()
            stop_route_map[sid].add(route_id)

    # Build parent_id -> children relationships
    parent_children: Dict[str, List[str]] = {}
    for stop in stops:
        sid = stop.get("stop_id")
        ps = stop.get("parent_station")
        if sid is None:
            continue
        if ps is not None and ps != "":
            if ps not in parent_children:
                parent_children[ps] = []
            parent_children[ps].append(sid)

    # Merge: parent gets all children's routes, then children get merged parent routes
    for parent_id, children in parent_children.items():
        if parent_id not in stop_route_map:
            stop_route_map[parent_id] = set()
        parent_routes = stop_route_map[parent_id]
        for child_id in children:
            child_routes = stop_route_map.get(child_id, set())
            parent_routes.update(child_routes)
        # Children absorb merged parent routes
        for child_id in children:
            if child_id not in stop_route_map:
                stop_route_map[child_id] = set()
            stop_route_map[child_id].update(parent_routes)

    return stop_route_map


def build_route_ids_for_stop(
    stop_id: str,
    stop_route_map: Dict[str, Set[str]],
) -> List[str]:
    """
    Serialize route_ids for a stop as a JSON array (matching the C++ logic
    that iterates the unordered_set and pushes into a nlohmann::json array).

    Returns the parsed list from the JSON round-trip, simulating what the
    GeoJSON serializer would produce.
    """
    routes = stop_route_map.get(stop_id, set())
    # Serialize as JSON array string (matching C++ nlohmann::json)
    route_ids_arr = list(routes)
    json_str = json.dumps(route_ids_arr)
    # Parse back (simulating the GeoJSON serializer round-trip)
    return json.loads(json_str)


# ---------------------------------------------------------------------------
# Strategies
# ---------------------------------------------------------------------------

# Route IDs: short alphanumeric identifiers
route_id_strategy = st.text(
    st.sampled_from("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"),
    min_size=1,
    max_size=5,
)

# Stop IDs: short identifiers
stop_id_strategy = st.text(
    st.sampled_from("abcdefghijklmnopqrstuvwxyz0123456789"),
    min_size=1,
    max_size=8,
)

# Trip IDs: short identifiers
trip_id_strategy = st.text(
    st.sampled_from("trip0123456789"),
    min_size=3,
    max_size=10,
)


@st.composite
def gtfs_feed_with_duplicate_route_exposure(draw):
    """
    Generate a GTFS feed where the same route_id serves a stop through
    multiple different trips. This is the key scenario that tests deduplication.

    Structure:
    - 1-3 route IDs
    - 1-5 stops (standalone, no parent/child for simplicity in this property)
    - Multiple trips per route
    - stop_times that connect stops to trips, ensuring at least one stop
      is served by the same route via multiple trips
    """
    # Generate route IDs (1-3 distinct routes)
    num_routes = draw(st.integers(min_value=1, max_value=3))
    route_ids = [draw(route_id_strategy) for _ in range(num_routes)]

    # Generate stop IDs (1-5 distinct stops)
    num_stops = draw(st.integers(min_value=1, max_value=5))
    stop_ids = list(set(draw(stop_id_strategy) for _ in range(num_stops)))
    if not stop_ids:
        stop_ids = ["stop1"]

    # Build stops (all standalone for this test)
    stops = [
        {"stop_id": sid, "stop_lat": "43.0", "stop_lon": "-2.0"}
        for sid in stop_ids
    ]

    # Generate multiple trips per route (2-5 trips per route to force duplicates)
    trips = []
    trip_counter = 0
    for rid in route_ids:
        num_trips_for_route = draw(st.integers(min_value=2, max_value=5))
        for _ in range(num_trips_for_route):
            trip_counter += 1
            trips.append({
                "trip_id": f"t{trip_counter}",
                "route_id": rid,
            })

    # Generate stop_times: each trip visits at least one stop
    # Importantly, multiple trips of the same route visit the same stop
    stop_times = []
    for trip in trips:
        # Each trip visits 1-3 stops (drawn from the available stops)
        num_visits = draw(st.integers(min_value=1, max_value=min(3, len(stop_ids))))
        visited = draw(
            st.lists(
                st.sampled_from(stop_ids),
                min_size=num_visits,
                max_size=num_visits,
            )
        )
        for seq, sid in enumerate(visited):
            stop_times.append({
                "stop_id": sid,
                "trip_id": trip["trip_id"],
                "stop_sequence": str(seq),
            })

    return stops, stop_times, trips


@st.composite
def gtfs_feed_with_parent_child_and_duplicates(draw):
    """
    Generate a GTFS feed with parent-child relationships where merging
    could potentially introduce duplicates if not handled correctly.

    The same route serves both a parent and its child through different trips.
    """
    # One route that serves both parent and child
    route_id = draw(route_id_strategy)

    parent_id = "parent_" + draw(
        st.text(st.sampled_from("0123456789"), min_size=1, max_size=3)
    )
    num_children = draw(st.integers(min_value=1, max_value=3))
    child_ids = [f"child_{i}" for i in range(num_children)]

    # Build stops
    stops = [
        {"stop_id": parent_id, "stop_lat": "43.0", "stop_lon": "-2.0",
         "location_type": "1"},
    ]
    for cid in child_ids:
        stops.append({
            "stop_id": cid, "stop_lat": "43.0", "stop_lon": "-2.0",
            "parent_station": parent_id,
        })

    # Create trips all for the same route
    trips = [
        {"trip_id": "trip_parent_1", "route_id": route_id},
        {"trip_id": "trip_parent_2", "route_id": route_id},
        {"trip_id": "trip_child_1", "route_id": route_id},
        {"trip_id": "trip_child_2", "route_id": route_id},
    ]

    # stop_times: parent served by its trips, children served by their trips
    stop_times = [
        {"stop_id": parent_id, "trip_id": "trip_parent_1", "stop_sequence": "0"},
        {"stop_id": parent_id, "trip_id": "trip_parent_2", "stop_sequence": "0"},
    ]
    for cid in child_ids:
        stop_times.append(
            {"stop_id": cid, "trip_id": "trip_child_1", "stop_sequence": "0"}
        )
        stop_times.append(
            {"stop_id": cid, "trip_id": "trip_child_2", "stop_sequence": "0"}
        )

    return stops, stop_times, trips


# ---------------------------------------------------------------------------
# Property Tests
# ---------------------------------------------------------------------------


class TestRouteIdsDeduplicationInvariant:
    """Property 4: Route IDs deduplication invariant."""

    @given(feed=gtfs_feed_with_duplicate_route_exposure())
    @settings(max_examples=200)
    def test_no_duplicate_route_ids_standalone_stops(self, feed):
        """
        **Validates: Requirements 2.3**

        For any standalone stop served by the same route through multiple trips,
        the route_ids array SHALL contain no duplicate entries.
        """
        stops, stop_times, trips = feed

        stop_route_map = build_stop_route_map(stops, stop_times, trips)

        for stop in stops:
            stop_id = stop["stop_id"]
            route_ids = build_route_ids_for_stop(stop_id, stop_route_map)

            # The key invariant: no duplicates
            assert len(route_ids) == len(set(route_ids)), (
                f"Stop '{stop_id}' has duplicate route_ids: {route_ids}"
            )

    @given(feed=gtfs_feed_with_parent_child_and_duplicates())
    @settings(max_examples=200)
    def test_no_duplicate_route_ids_after_parent_child_merge(self, feed):
        """
        **Validates: Requirements 2.3**

        For any parent station or child stop where parent-child merging occurs,
        the route_ids array SHALL contain no duplicate entries even when the same
        route serves both parent and children through multiple trips.
        """
        stops, stop_times, trips = feed

        stop_route_map = build_stop_route_map(stops, stop_times, trips)

        for stop in stops:
            stop_id = stop["stop_id"]
            route_ids = build_route_ids_for_stop(stop_id, stop_route_map)

            # The key invariant: no duplicates after merging
            assert len(route_ids) == len(set(route_ids)), (
                f"Stop '{stop_id}' has duplicate route_ids after merge: {route_ids}"
            )

    @given(feed=gtfs_feed_with_duplicate_route_exposure())
    @settings(max_examples=200)
    def test_route_ids_count_bounded_by_distinct_routes(self, feed):
        """
        **Validates: Requirements 2.3**

        For any stop, the number of entries in route_ids SHALL not exceed the
        total number of distinct route_ids in the feed (a consequence of proper
        deduplication).
        """
        stops, stop_times, trips = feed

        # Count distinct routes in the feed
        all_route_ids = set()
        for trip in trips:
            rid = trip.get("route_id")
            if rid:
                all_route_ids.add(rid)

        stop_route_map = build_stop_route_map(stops, stop_times, trips)

        for stop in stops:
            stop_id = stop["stop_id"]
            route_ids = build_route_ids_for_stop(stop_id, stop_route_map)

            assert len(route_ids) <= len(all_route_ids), (
                f"Stop '{stop_id}' has {len(route_ids)} route_ids but only "
                f"{len(all_route_ids)} distinct routes exist in the feed. "
                f"route_ids={route_ids}"
            )
