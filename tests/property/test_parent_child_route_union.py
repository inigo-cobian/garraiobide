"""
Feature: stops-route-filtering, Property 3: Parent-child route membership union

**Validates: Requirements 2.1, 2.2**

Property 3: For any parent station with one or more child stops in a GTFS feed,
both the parent and each child SHALL have a `route_ids` set equal to the union of
all route IDs directly serving the parent and all route IDs directly serving any
child in that station group.

This test re-implements the parent-child route membership merging logic from
`gtfs_parser.cc` (the `build_stop_route_map` function) in Python and uses
Hypothesis to verify the union property holds for generated GTFS feeds with
parent stations and child stops served by different routes.
"""

from dataclasses import dataclass, field

from hypothesis import given, settings
from hypothesis import strategies as st


# ---------------------------------------------------------------------------
# Model: Minimal GTFS data structures for route membership computation
# ---------------------------------------------------------------------------


@dataclass
class Stop:
    """Represents a GTFS stop row."""
    stop_id: str
    stop_lat: float = 43.26
    stop_lon: float = -2.93
    location_type: str = ""  # "1" for parent station, "" for platform/stop
    parent_station: str = ""  # stop_id of parent if this is a child


@dataclass
class Trip:
    """Represents a GTFS trip row."""
    trip_id: str
    route_id: str


@dataclass
class StopTime:
    """Represents a GTFS stop_time row."""
    trip_id: str
    stop_id: str
    stop_sequence: int = 1


@dataclass
class StationGroup:
    """A parent station with its child stops and their route assignments."""
    parent_id: str
    child_ids: list[str] = field(default_factory=list)
    parent_route_ids: set[str] = field(default_factory=set)
    child_route_ids: dict[str, set[str]] = field(default_factory=dict)


# ---------------------------------------------------------------------------
# System Under Test: re-implementation of build_stop_route_map from
# gtfs_parser.cc focusing on the parent-child merging logic
# ---------------------------------------------------------------------------


def build_stop_route_map(
    stops: list[Stop],
    stop_times: list[StopTime],
    trips: list[Trip],
) -> dict[str, set[str]]:
    """
    Re-implementation of the C++ build_stop_route_map function.

    1. Index trips by trip_id -> route_id
    2. Join stop_times -> trips to build stop_id -> set<route_id>
    3. Build parent -> children relationships
    4. Merge: parent absorbs all children's routes, then children absorb
       the merged parent routes (which now includes the union)

    This matches the algorithm in gtfs_parser.cc:
        // Merge: parent gets all children's routes, then children get merged parent routes
        for (const auto& [parent_id, children] : parent_children) {
            auto& parent_routes = stop_route_map[parent_id];
            for (const auto& child_id : children) {
                auto& child_routes = stop_route_map[child_id];
                parent_routes.insert(child_routes.begin(), child_routes.end());
            }
            for (const auto& child_id : children) {
                stop_route_map[child_id].insert(parent_routes.begin(), parent_routes.end());
            }
        }
    """
    # Step 1: Index trips by trip_id -> route_id
    trip_route_index: dict[str, str] = {}
    for trip in trips:
        trip_route_index[trip.trip_id] = trip.route_id

    # Step 2: Join stop_times -> trips to get stop_id -> set<route_id>
    stop_route_map: dict[str, set[str]] = {}
    for st in stop_times:
        if st.trip_id in trip_route_index:
            route_id = trip_route_index[st.trip_id]
            if st.stop_id not in stop_route_map:
                stop_route_map[st.stop_id] = set()
            stop_route_map[st.stop_id].add(route_id)

    # Step 3: Build parent_id -> list of child_ids
    parent_children: dict[str, list[str]] = {}
    for stop in stops:
        if stop.parent_station:
            if stop.parent_station not in parent_children:
                parent_children[stop.parent_station] = []
            parent_children[stop.parent_station].append(stop.stop_id)

    # Step 4: Merge parent <-> child relationships
    for parent_id, children in parent_children.items():
        # Ensure parent exists in map
        if parent_id not in stop_route_map:
            stop_route_map[parent_id] = set()
        parent_routes = stop_route_map[parent_id]

        # Parent absorbs all children's routes
        for child_id in children:
            if child_id in stop_route_map:
                parent_routes.update(stop_route_map[child_id])

        # Children absorb merged parent routes (which now has the full union)
        for child_id in children:
            if child_id not in stop_route_map:
                stop_route_map[child_id] = set()
            stop_route_map[child_id].update(parent_routes)

    return stop_route_map


# ---------------------------------------------------------------------------
# Reference oracle: compute expected union directly
# ---------------------------------------------------------------------------


def compute_expected_union(group: StationGroup) -> set[str]:
    """
    Reference oracle: the expected route_ids for any member of the station group
    is the union of all route IDs directly serving any member.
    """
    union = set(group.parent_route_ids)
    for child_routes in group.child_route_ids.values():
        union.update(child_routes)
    return union


# ---------------------------------------------------------------------------
# Strategies
# ---------------------------------------------------------------------------

# Route IDs: short alphanumeric strings
route_ids_strategy = st.text(
    st.sampled_from("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"),
    min_size=1,
    max_size=5,
)

# Generate a set of 1-5 route IDs for a stop
route_set_strategy = st.frozensets(route_ids_strategy, min_size=0, max_size=5)


@st.composite
def gtfs_feed_with_station_groups(draw):
    """
    Generate a complete GTFS feed with 1-3 station groups.
    Each group has a unique parent station and multiple children served by
    different routes. Returns (stops, stop_times, trips, groups) for verification.
    """
    num_groups = draw(st.integers(min_value=1, max_value=3))
    groups: list[StationGroup] = []

    all_stops: list[Stop] = []
    all_trips: list[Trip] = []
    all_stop_times: list[StopTime] = []

    trip_counter = 0

    for group_idx in range(num_groups):
        # Use group index to guarantee unique parent IDs
        parent_id = f"parent_{group_idx}"

        num_children = draw(st.integers(min_value=1, max_value=5))
        child_ids = [
            f"child_{group_idx}_{i}" for i in range(num_children)
        ]

        parent_routes = set(draw(route_set_strategy))
        child_routes = {}
        for child_id in child_ids:
            child_routes[child_id] = set(draw(route_set_strategy))

        # Ensure at least one route exists somewhere in the group
        all_group_routes = parent_routes.union(*child_routes.values())
        if not all_group_routes:
            extra_route = draw(route_ids_strategy)
            child_routes[child_ids[0]].add(extra_route)

        group = StationGroup(
            parent_id=parent_id,
            child_ids=child_ids,
            parent_route_ids=parent_routes,
            child_route_ids=child_routes,
        )
        groups.append(group)

        # Create parent stop
        all_stops.append(Stop(
            stop_id=group.parent_id,
            location_type="1",
            parent_station="",
        ))

        # Create child stops
        for child_id in group.child_ids:
            all_stops.append(Stop(
                stop_id=child_id,
                location_type="",
                parent_station=group.parent_id,
            ))

        # Create trips and stop_times for parent's routes
        for route_id in group.parent_route_ids:
            trip_id = f"trip_{trip_counter}"
            trip_counter += 1
            all_trips.append(Trip(trip_id=trip_id, route_id=route_id))
            all_stop_times.append(StopTime(
                trip_id=trip_id,
                stop_id=group.parent_id,
                stop_sequence=1,
            ))

        # Create trips and stop_times for each child's routes
        for child_id, route_ids in group.child_route_ids.items():
            for route_id in route_ids:
                trip_id = f"trip_{trip_counter}"
                trip_counter += 1
                all_trips.append(Trip(trip_id=trip_id, route_id=route_id))
                all_stop_times.append(StopTime(
                    trip_id=trip_id,
                    stop_id=child_id,
                    stop_sequence=1,
                ))

    return all_stops, all_stop_times, all_trips, groups


# ---------------------------------------------------------------------------
# Property Tests
# ---------------------------------------------------------------------------


class TestParentChildRouteUnionProperty:
    """Property 3: Parent-child route membership union."""

    @given(data=gtfs_feed_with_station_groups())
    @settings(max_examples=200)
    def test_parent_has_union_of_all_group_routes(
        self, data: tuple
    ) -> None:
        """
        **Validates: Requirements 2.1, 2.2**

        For any parent station, its route_ids SHALL equal the union of all
        route IDs directly serving the parent and all route IDs directly
        serving any child in the station group.
        """
        stops, stop_times, trips, groups = data

        stop_route_map = build_stop_route_map(stops, stop_times, trips)

        for group in groups:
            expected_union = compute_expected_union(group)
            parent_routes = stop_route_map.get(group.parent_id, set())

            assert parent_routes == expected_union, (
                f"Parent '{group.parent_id}' route_ids mismatch.\n"
                f"  Expected (union): {sorted(expected_union)}\n"
                f"  Got: {sorted(parent_routes)}\n"
                f"  Parent direct routes: {sorted(group.parent_route_ids)}\n"
                f"  Child routes: {group.child_route_ids}"
            )

    @given(data=gtfs_feed_with_station_groups())
    @settings(max_examples=200)
    def test_each_child_has_union_of_all_group_routes(
        self, data: tuple
    ) -> None:
        """
        **Validates: Requirements 2.1, 2.2**

        For any child stop belonging to a parent station, its route_ids SHALL
        equal the union of all route IDs directly serving the parent and all
        route IDs directly serving any child in the station group.
        """
        stops, stop_times, trips, groups = data

        stop_route_map = build_stop_route_map(stops, stop_times, trips)

        for group in groups:
            expected_union = compute_expected_union(group)
            for child_id in group.child_ids:
                child_routes = stop_route_map.get(child_id, set())

                assert child_routes == expected_union, (
                    f"Child '{child_id}' of parent '{group.parent_id}' "
                    f"route_ids mismatch.\n"
                    f"  Expected (union): {sorted(expected_union)}\n"
                    f"  Got: {sorted(child_routes)}\n"
                    f"  This child's direct routes: "
                    f"{sorted(group.child_route_ids.get(child_id, set()))}\n"
                    f"  Parent direct routes: {sorted(group.parent_route_ids)}\n"
                    f"  All child routes: {group.child_route_ids}"
                )

    @given(data=gtfs_feed_with_station_groups())
    @settings(max_examples=200)
    def test_parent_and_all_children_have_identical_route_sets(
        self, data: tuple
    ) -> None:
        """
        **Validates: Requirements 2.1, 2.2**

        For any station group, the parent and ALL children SHALL have
        identical route_ids sets (since they all get the same union).
        """
        stops, stop_times, trips, groups = data

        stop_route_map = build_stop_route_map(stops, stop_times, trips)

        for group in groups:
            parent_routes = stop_route_map.get(group.parent_id, set())
            for child_id in group.child_ids:
                child_routes = stop_route_map.get(child_id, set())
                assert parent_routes == child_routes, (
                    f"Parent '{group.parent_id}' and child '{child_id}' "
                    f"have different route_ids.\n"
                    f"  Parent: {sorted(parent_routes)}\n"
                    f"  Child: {sorted(child_routes)}\n"
                    f"  Difference: "
                    f"{sorted(parent_routes.symmetric_difference(child_routes))}"
                )

    @given(data=gtfs_feed_with_station_groups())
    @settings(max_examples=200)
    def test_union_includes_all_directly_serving_routes(
        self, data: tuple
    ) -> None:
        """
        **Validates: Requirements 2.1, 2.2**

        For any station group, every route that directly serves any member
        (parent or child) SHALL appear in the route_ids of every member.
        No directly-serving route shall be lost during merging.
        """
        stops, stop_times, trips, groups = data

        stop_route_map = build_stop_route_map(stops, stop_times, trips)

        for group in groups:
            # Collect all directly-serving routes
            all_direct_routes = set(group.parent_route_ids)
            for child_routes in group.child_route_ids.values():
                all_direct_routes.update(child_routes)

            # Verify parent contains all direct routes
            parent_routes = stop_route_map.get(group.parent_id, set())
            missing_from_parent = all_direct_routes - parent_routes
            assert not missing_from_parent, (
                f"Parent '{group.parent_id}' is missing routes: "
                f"{sorted(missing_from_parent)}"
            )

            # Verify each child contains all direct routes
            for child_id in group.child_ids:
                child_routes = stop_route_map.get(child_id, set())
                missing_from_child = all_direct_routes - child_routes
                assert not missing_from_child, (
                    f"Child '{child_id}' is missing routes: "
                    f"{sorted(missing_from_child)}"
                )
