"""
Feature: stops-route-filtering, Property 7: Orphan stop always-visible guarantee

**Validates: Requirements 6.1, 6.2**

Property 7: For any stop feature whose `route_ids` property is either missing,
null, not an array, or an empty array, and for any route visibility state and
display mode, the stop SHALL always be visible.

This test generates stop features with various "orphan" route_ids configurations
(missing, None, non-array types, empty array) combined with arbitrary route
visibility maps and display modes, and verifies that the isStopVisible function
always returns True for these cases.
"""

from typing import Any, Dict, Optional

from hypothesis import given, settings
from hypothesis import strategies as st


# ---------------------------------------------------------------------------
# System Under Test: Python re-implementation of isStopVisible from app.js
# ---------------------------------------------------------------------------


def is_stop_visible(feature: Dict[str, Any], route_vis: Dict[str, bool], mode: str) -> bool:
    """
    Python re-implementation of the JavaScript isStopVisible function.

    Determines if a stop feature should be visible given current route visibility.

    Args:
        feature: GeoJSON feature dict with properties.route_ids
        route_vis: {routeId: bool} visibility map
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
    # Mirrors JS: if (!routeIds || !Array.isArray(routeIds) || routeIds.length === 0)
    if not route_ids or not isinstance(route_ids, list) or len(route_ids) == 0:
        return True

    # A stop is visible if at least one of its route_ids is visible
    for rid in route_ids:
        if route_vis.get(rid) is not False:
            return True

    return False


# ---------------------------------------------------------------------------
# Strategies
# ---------------------------------------------------------------------------

# Route IDs for visibility maps
route_id_strategy = st.text(
    st.sampled_from("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_"),
    min_size=1,
    max_size=6,
)

# Route visibility map: arbitrary mapping of route IDs to booleans
route_visibility_strategy = st.dictionaries(
    keys=route_id_strategy,
    values=st.booleans(),
    min_size=0,
    max_size=10,
)

# Display modes
mode_strategy = st.sampled_from(["both", "stops", "lines"])

# Non-array route_ids values (simulating various invalid/missing states)
non_array_route_ids_strategy = st.one_of(
    st.just(None),                          # null/None
    st.text(min_size=0, max_size=20),       # string (including empty string)
    st.integers(),                           # number
    st.floats(allow_nan=False, allow_infinity=False),  # float
    st.booleans(),                           # boolean
    st.dictionaries(                         # object
        keys=st.text(min_size=1, max_size=5),
        values=st.text(min_size=0, max_size=5),
        max_size=3,
    ),
)


@st.composite
def orphan_stop_feature_missing_route_ids(draw):
    """Generate a stop feature where route_ids is completely absent from properties."""
    props = {
        "stop_name": draw(st.text(min_size=1, max_size=20)),
        "stop_type": draw(st.sampled_from(["parent_station", "child_stop", "stop"])),
    }
    # Explicitly do NOT include route_ids
    return {
        "type": "Feature",
        "geometry": {"type": "Point", "coordinates": [draw(st.floats(-180, 180)), draw(st.floats(-90, 90))]},
        "properties": props,
    }


@st.composite
def orphan_stop_feature_null_route_ids(draw):
    """Generate a stop feature where route_ids is None/null."""
    props = {
        "stop_name": draw(st.text(min_size=1, max_size=20)),
        "stop_type": draw(st.sampled_from(["parent_station", "child_stop", "stop"])),
        "route_ids": None,
    }
    return {
        "type": "Feature",
        "geometry": {"type": "Point", "coordinates": [draw(st.floats(-180, 180)), draw(st.floats(-90, 90))]},
        "properties": props,
    }


@st.composite
def orphan_stop_feature_empty_route_ids(draw):
    """Generate a stop feature where route_ids is an empty array."""
    props = {
        "stop_name": draw(st.text(min_size=1, max_size=20)),
        "stop_type": draw(st.sampled_from(["parent_station", "child_stop", "stop"])),
        "route_ids": [],
    }
    return {
        "type": "Feature",
        "geometry": {"type": "Point", "coordinates": [draw(st.floats(-180, 180)), draw(st.floats(-90, 90))]},
        "properties": props,
    }


@st.composite
def orphan_stop_feature_non_array_route_ids(draw):
    """Generate a stop feature where route_ids is a non-array value (string, number, object, etc.)."""
    non_array_value = draw(non_array_route_ids_strategy)
    props = {
        "stop_name": draw(st.text(min_size=1, max_size=20)),
        "stop_type": draw(st.sampled_from(["parent_station", "child_stop", "stop"])),
        "route_ids": non_array_value,
    }
    return {
        "type": "Feature",
        "geometry": {"type": "Point", "coordinates": [draw(st.floats(-180, 180)), draw(st.floats(-90, 90))]},
        "properties": props,
    }


# Combined strategy for all orphan stop variants
orphan_stop_feature_strategy = st.one_of(
    orphan_stop_feature_missing_route_ids(),
    orphan_stop_feature_null_route_ids(),
    orphan_stop_feature_empty_route_ids(),
    orphan_stop_feature_non_array_route_ids(),
)


# ---------------------------------------------------------------------------
# Property Tests
# ---------------------------------------------------------------------------


class TestOrphanStopAlwaysVisibleGuarantee:
    """Property 7: Orphan stop always-visible guarantee."""

    @given(
        feature=orphan_stop_feature_missing_route_ids(),
        route_vis=route_visibility_strategy,
        mode=mode_strategy,
    )
    @settings(max_examples=200)
    def test_missing_route_ids_always_visible(self, feature, route_vis, mode):
        """
        **Validates: Requirements 6.2**

        For any stop feature missing the route_ids property, and for any route
        visibility state and display mode, the stop SHALL always be visible.
        """
        assert is_stop_visible(feature, route_vis, mode) is True, (
            f"Stop with missing route_ids should always be visible. "
            f"mode={mode}, route_vis={route_vis}, feature={feature}"
        )

    @given(
        feature=orphan_stop_feature_null_route_ids(),
        route_vis=route_visibility_strategy,
        mode=mode_strategy,
    )
    @settings(max_examples=200)
    def test_null_route_ids_always_visible(self, feature, route_vis, mode):
        """
        **Validates: Requirements 6.1, 6.2**

        For any stop feature where route_ids is null/None, and for any route
        visibility state and display mode, the stop SHALL always be visible.
        """
        assert is_stop_visible(feature, route_vis, mode) is True, (
            f"Stop with null route_ids should always be visible. "
            f"mode={mode}, route_vis={route_vis}, feature={feature}"
        )

    @given(
        feature=orphan_stop_feature_empty_route_ids(),
        route_vis=route_visibility_strategy,
        mode=mode_strategy,
    )
    @settings(max_examples=200)
    def test_empty_route_ids_always_visible(self, feature, route_vis, mode):
        """
        **Validates: Requirements 6.1**

        For any stop feature where route_ids is an empty array, and for any
        route visibility state and display mode, the stop SHALL always be visible.
        """
        assert is_stop_visible(feature, route_vis, mode) is True, (
            f"Stop with empty route_ids should always be visible. "
            f"mode={mode}, route_vis={route_vis}, feature={feature}"
        )

    @given(
        feature=orphan_stop_feature_non_array_route_ids(),
        route_vis=route_visibility_strategy,
        mode=mode_strategy,
    )
    @settings(max_examples=200)
    def test_non_array_route_ids_always_visible(self, feature, route_vis, mode):
        """
        **Validates: Requirements 6.1, 6.2**

        For any stop feature where route_ids is not an array (e.g., string,
        number, object, boolean), and for any route visibility state and
        display mode, the stop SHALL always be visible.
        """
        assert is_stop_visible(feature, route_vis, mode) is True, (
            f"Stop with non-array route_ids should always be visible. "
            f"mode={mode}, route_vis={route_vis}, "
            f"route_ids={feature.get('properties', {}).get('route_ids')}"
        )

    @given(
        feature=orphan_stop_feature_strategy,
        route_vis=route_visibility_strategy,
        mode=mode_strategy,
    )
    @settings(max_examples=500)
    def test_all_orphan_variants_always_visible(self, feature, route_vis, mode):
        """
        **Validates: Requirements 6.1, 6.2**

        Combined property: for any stop feature whose route_ids property is
        missing, null, not an array, or an empty array, and for any route
        visibility state and display mode, the stop SHALL always be visible.
        """
        assert is_stop_visible(feature, route_vis, mode) is True, (
            f"Orphan stop should always be visible. "
            f"mode={mode}, route_vis={route_vis}, "
            f"route_ids={feature.get('properties', {}).get('route_ids')}"
        )
