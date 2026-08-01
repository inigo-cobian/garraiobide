"""
Property test for Only Stops mode bypass.

Feature: stops-route-filtering, Property 6: Only Stops mode bypass

**Validates: Requirements 4.1**

For any stop feature and for any route visibility state, when the display mode
is 'stops' (Only Stops Mode), the stop SHALL be visible regardless of its
route_ids content or the route visibility map.
"""

from typing import Any, Optional

import hypothesis.strategies as st
from hypothesis import given, settings


# ---------------------------------------------------------------------------
# Python model of isStopVisible from frontend/app.js
# ---------------------------------------------------------------------------


def is_stop_visible(feature: dict[str, Any], route_vis: dict[str, bool], mode: str) -> bool:
    """
    Python re-implementation of the frontend isStopVisible function.

    Determines if a stop feature should be visible given current route visibility.

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

    # No route_ids or empty array → always visible
    if route_ids is None or not isinstance(route_ids, list) or len(route_ids) == 0:
        return True

    # A stop is visible if at least one of its route_ids is visible
    for rid in route_ids:
        if route_vis.get(rid) is not False:
            return True

    return False


# ---------------------------------------------------------------------------
# Hypothesis strategies
# ---------------------------------------------------------------------------

# Strategy for route ID strings (non-empty alphanumeric)
route_id_strategy = st.text(
    alphabet=st.characters(categories=("L", "N")),
    min_size=1,
    max_size=10,
)

# Strategy for route_ids property values: can be a list of strings, empty list,
# None, or non-array types to cover all edge cases
route_ids_value_strategy = st.one_of(
    # Non-empty list of route IDs
    st.lists(route_id_strategy, min_size=1, max_size=10),
    # Empty list
    st.just([]),
    # None / missing
    st.just(None),
    # Non-array types that might appear as route_ids
    st.just("not_an_array"),
    st.just(42),
    st.just(True),
    st.just({}),
)


@st.composite
def stop_feature_strategy(draw: st.DrawFn) -> dict[str, Any]:
    """
    Generate an arbitrary stop feature with various route_ids values.

    The feature may have route_ids as a list of strings, an empty list,
    None, or various non-array types.
    """
    route_ids_value = draw(route_ids_value_strategy)

    properties: dict[str, Any] = {
        "stop_name": draw(st.text(min_size=0, max_size=20)),
        "stop_type": draw(st.sampled_from(["parent_station", "child_stop", "standalone"])),
    }

    # Decide whether to include route_ids in properties at all
    include_route_ids = draw(st.booleans())
    if include_route_ids and route_ids_value is not None:
        properties["route_ids"] = route_ids_value

    return {
        "type": "Feature",
        "id": draw(st.text(min_size=1, max_size=10)),
        "geometry": {"type": "Point", "coordinates": [-2.93, 43.26]},
        "properties": properties,
    }


@st.composite
def route_visibility_strategy(draw: st.DrawFn) -> dict[str, bool]:
    """
    Generate an arbitrary route visibility map.

    Maps route IDs to boolean visibility states. May include routes that
    are all hidden, all visible, or a mix.
    """
    num_routes = draw(st.integers(min_value=0, max_value=10))
    route_vis: dict[str, bool] = {}
    for _ in range(num_routes):
        rid = draw(route_id_strategy)
        visible = draw(st.booleans())
        route_vis[rid] = visible
    return route_vis


# ---------------------------------------------------------------------------
# Property tests
# ---------------------------------------------------------------------------


class TestOnlyStopsModeBypass:
    """Property 6: Only Stops mode bypass."""

    @given(
        feature=stop_feature_strategy(),
        route_vis=route_visibility_strategy(),
    )
    @settings(max_examples=500)
    def test_stops_mode_always_returns_visible(
        self,
        feature: dict[str, Any],
        route_vis: dict[str, bool],
    ) -> None:
        """
        **Validates: Requirements 4.1**

        For any stop feature and for any route visibility state, when the
        display mode is 'stops' (Only Stops Mode), the stop SHALL be visible
        regardless of its route_ids content or the route visibility map.
        """
        result = is_stop_visible(feature, route_vis, mode="stops")

        assert result is True, (
            f"Stop should always be visible in 'stops' mode.\n"
            f"Feature route_ids: {feature.get('properties', {}).get('route_ids')}\n"
            f"Route visibility: {route_vis}\n"
            f"Got: {result}"
        )

    @given(
        feature=stop_feature_strategy(),
        route_vis=route_visibility_strategy(),
    )
    @settings(max_examples=200)
    def test_stops_mode_ignores_all_routes_hidden(
        self,
        feature: dict[str, Any],
        route_vis: dict[str, bool],
    ) -> None:
        """
        **Validates: Requirements 4.1**

        Even when all routes in the visibility map are set to False (hidden),
        stops mode still returns True (visible).
        """
        # Force all routes to hidden
        all_hidden_vis = {k: False for k in route_vis}

        result = is_stop_visible(feature, all_hidden_vis, mode="stops")

        assert result is True, (
            f"Stop should be visible in 'stops' mode even with all routes hidden.\n"
            f"Feature route_ids: {feature.get('properties', {}).get('route_ids')}\n"
            f"All-hidden route vis: {all_hidden_vis}\n"
            f"Got: {result}"
        )

    @given(
        route_ids=st.lists(route_id_strategy, min_size=1, max_size=10),
    )
    @settings(max_examples=200)
    def test_stops_mode_bypass_with_matching_hidden_routes(
        self,
        route_ids: list[str],
    ) -> None:
        """
        **Validates: Requirements 4.1**

        When a stop's route_ids are all explicitly hidden in the visibility map,
        the stop is still visible in 'stops' mode — confirming the mode takes
        precedence over route visibility.
        """
        # Build a feature with specific route_ids
        feature = {
            "type": "Feature",
            "id": "test_stop",
            "geometry": {"type": "Point", "coordinates": [0, 0]},
            "properties": {
                "stop_name": "Test Stop",
                "stop_type": "parent_station",
                "route_ids": route_ids,
            },
        }

        # Build a route_vis where all the stop's routes are hidden
        route_vis = {rid: False for rid in route_ids}

        result = is_stop_visible(feature, route_vis, mode="stops")

        assert result is True, (
            f"Stop should be visible in 'stops' mode even when all its routes are hidden.\n"
            f"route_ids: {route_ids}\n"
            f"route_vis: {route_vis}\n"
            f"Got: {result}"
        )
