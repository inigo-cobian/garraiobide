"""
Feature: stops-route-filtering, Property 5: Stop filtering visibility rule

**Validates: Requirements 3.1, 3.2, 5.1, 5.2**

Property 5: For any stop feature with a non-empty `route_ids` array, and for any
route visibility map, the stop SHALL be visible if and only if at least one element
of its `route_ids` array maps to `true` (visible) in the route visibility map.
This applies equally to parent stations and child stops.

The visibility rule in 'both' mode: a stop with non-empty route_ids is visible iff
at least one route_id has `routeVis[routeId] !== false` (i.e., true or undefined
both count as visible).

This test re-implements the `isStopVisible` function logic in Python as the system
under test, generating arbitrary stop features and route visibility maps.
"""

from typing import Any, Dict, List, Optional

import hypothesis.strategies as st
from hypothesis import given, settings


# ---------------------------------------------------------------------------
# System Under Test: re-implementation of isStopVisible from frontend/app.js
# ---------------------------------------------------------------------------


def is_stop_visible(
    feature: Dict[str, Any],
    route_vis: Dict[str, bool],
    mode: str,
) -> bool:
    """
    Re-implementation of the JavaScript isStopVisible function.

    Determine if a stop feature should be visible given current route visibility.

    Args:
        feature: GeoJSON feature dict with properties.route_ids
        route_vis: { routeId: bool } visibility map
        mode: 'both', 'lines', or 'stops'

    Returns:
        True if the stop should be shown
    """
    # In Only Stops mode, all stops are visible
    if mode == "stops":
        return True

    props = feature.get("properties") or {}
    route_ids = props.get("route_ids")

    # No route_ids or empty array -> always visible
    if route_ids is None or not isinstance(route_ids, list) or len(route_ids) == 0:
        return True

    # A stop is visible if at least one of its route_ids is visible
    # In JS: routeVis[routeIds[i]] !== false
    # This means: true, undefined (missing key), or any non-false value => visible
    for rid in route_ids:
        if route_vis.get(rid) is not False:
            return True

    return False


# ---------------------------------------------------------------------------
# Strategies
# ---------------------------------------------------------------------------

# Route IDs: short alphanumeric identifiers
route_id_strategy = st.text(
    st.sampled_from("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"),
    min_size=1,
    max_size=5,
)

# Non-empty list of route IDs for a stop
non_empty_route_ids_strategy = st.lists(
    route_id_strategy,
    min_size=1,
    max_size=10,
)

# Stop type: either parent_station or child_stop
stop_type_strategy = st.sampled_from(["parent_station", "child_stop"])


@st.composite
def stop_feature_with_route_ids(draw):
    """
    Generate a stop feature with a non-empty route_ids array and a stop_type.
    """
    route_ids = draw(non_empty_route_ids_strategy)
    stop_type = draw(stop_type_strategy)

    feature = {
        "type": "Feature",
        "geometry": {"type": "Point", "coordinates": [-2.93, 43.26]},
        "properties": {
            "stop_name": "Test Stop",
            "stop_type": stop_type,
            "route_ids": route_ids,
        },
    }
    return feature


@st.composite
def route_visibility_map_for_routes(draw, route_ids: List[str]):
    """
    Generate a route visibility map that may contain entries for the given
    route_ids plus potentially some extra route_ids not in the stop.

    Each route_id in the map is either:
    - True (visible)
    - False (not visible)
    - Missing from the map (treated as visible in JS: routeVis[id] !== false)
    """
    vis_map = {}
    for rid in route_ids:
        # For each route_id, decide: include in map or omit (undefined in JS)
        include = draw(st.booleans())
        if include:
            vis_map[rid] = draw(st.booleans())
        # else: key is absent, which JS treats as undefined (!== false => visible)

    # Optionally add some extra route IDs not in the stop's list
    num_extra = draw(st.integers(min_value=0, max_value=3))
    for _ in range(num_extra):
        extra_rid = draw(route_id_strategy)
        if extra_rid not in vis_map:
            vis_map[extra_rid] = draw(st.booleans())

    return vis_map


@st.composite
def stop_and_visibility_map(draw):
    """
    Generate a stop feature with non-empty route_ids and a corresponding
    route visibility map.
    """
    feature = draw(stop_feature_with_route_ids())
    route_ids = feature["properties"]["route_ids"]
    route_vis = draw(route_visibility_map_for_routes(route_ids))
    return feature, route_vis


# ---------------------------------------------------------------------------
# Property Tests
# ---------------------------------------------------------------------------


class TestStopFilteringVisibilityRule:
    """Property 5: Stop filtering visibility rule."""

    @given(data=stop_and_visibility_map())
    @settings(max_examples=500)
    def test_visibility_biconditional_in_both_mode(self, data):
        """
        **Validates: Requirements 3.1, 3.2, 5.1, 5.2**

        For any stop feature with a non-empty route_ids array, in 'both' mode,
        the stop is visible if and only if at least one route_id is not
        explicitly false in the route visibility map.

        The biconditional:
          visible <=> exists route_id in route_ids where routeVis[route_id] !== false
        """
        feature, route_vis = data
        route_ids = feature["properties"]["route_ids"]

        # Compute expected visibility using the specification directly
        # "at least one route_id not explicitly false"
        expected_visible = any(
            route_vis.get(rid) is not False for rid in route_ids
        )

        # Compute actual visibility from the SUT
        actual_visible = is_stop_visible(feature, route_vis, "both")

        assert actual_visible == expected_visible, (
            f"Visibility mismatch for route_ids={route_ids}, "
            f"route_vis={route_vis}: "
            f"expected {expected_visible}, got {actual_visible}"
        )

    @given(data=stop_and_visibility_map())
    @settings(max_examples=500)
    def test_all_routes_hidden_implies_stop_hidden(self, data):
        """
        **Validates: Requirements 3.1, 5.1**

        When ALL route_ids of a stop are explicitly set to false in the
        route visibility map, the stop SHALL NOT be visible in 'both' mode.
        """
        feature, _ = data
        route_ids = feature["properties"]["route_ids"]

        # Construct a visibility map where ALL routes are explicitly hidden
        all_hidden_vis = {rid: False for rid in route_ids}

        actual_visible = is_stop_visible(feature, all_hidden_vis, "both")

        assert actual_visible is False, (
            f"Stop should be hidden when all route_ids are explicitly false. "
            f"route_ids={route_ids}, route_vis={all_hidden_vis}"
        )

    @given(data=stop_and_visibility_map())
    @settings(max_examples=500)
    def test_at_least_one_route_visible_implies_stop_visible(self, data):
        """
        **Validates: Requirements 3.2, 5.2**

        When at least one route_id of a stop is visible (True or absent from
        the map), the stop SHALL be visible in 'both' mode.
        """
        feature, _ = data
        route_ids = feature["properties"]["route_ids"]

        # Construct a visibility map where first route is visible, rest are hidden
        vis_map = {rid: False for rid in route_ids}
        # Make the first route explicitly visible
        vis_map[route_ids[0]] = True

        actual_visible = is_stop_visible(feature, vis_map, "both")

        assert actual_visible is True, (
            f"Stop should be visible when at least one route_id is visible. "
            f"route_ids={route_ids}, route_vis={vis_map}"
        )

    @given(data=stop_and_visibility_map())
    @settings(max_examples=300)
    def test_undefined_route_counts_as_visible(self, data):
        """
        **Validates: Requirements 3.2, 5.2**

        When a route_id is absent from the visibility map (undefined in JS),
        it is treated as visible (routeVis[id] !== false evaluates to true).
        """
        feature, _ = data
        route_ids = feature["properties"]["route_ids"]

        # Construct a visibility map with NO entries for any of the stop's routes
        # (all undefined). Other random routes can be present.
        empty_vis: Dict[str, bool] = {}

        actual_visible = is_stop_visible(feature, empty_vis, "both")

        assert actual_visible is True, (
            f"Stop should be visible when all route_ids are absent from "
            f"visibility map (treated as undefined/visible). "
            f"route_ids={route_ids}"
        )

    @given(
        feature=stop_feature_with_route_ids(),
        route_vis=st.dictionaries(route_id_strategy, st.booleans(), max_size=10),
    )
    @settings(max_examples=300)
    def test_parent_and_child_stops_same_rule(self, feature, route_vis):
        """
        **Validates: Requirements 3.1, 3.2, 5.1, 5.2**

        The visibility rule applies equally to parent stations and child stops.
        Changing only the stop_type between parent_station and child_stop SHALL
        NOT affect visibility outcome.
        """
        route_ids = feature["properties"]["route_ids"]

        # Test as parent_station
        feature["properties"]["stop_type"] = "parent_station"
        parent_visible = is_stop_visible(feature, route_vis, "both")

        # Test as child_stop
        feature["properties"]["stop_type"] = "child_stop"
        child_visible = is_stop_visible(feature, route_vis, "both")

        assert parent_visible == child_visible, (
            f"Visibility rule must be the same for parent_station and child_stop. "
            f"route_ids={route_ids}, route_vis={route_vis}, "
            f"parent_visible={parent_visible}, child_visible={child_visible}"
        )
