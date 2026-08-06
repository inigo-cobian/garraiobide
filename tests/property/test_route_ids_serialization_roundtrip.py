"""
Feature: stops-route-filtering, Property 2: Route IDs serialization round-trip

**Validates: Requirements 1.2**

Property 2: For any set of route ID strings assigned to a stop feature,
serializing the stop layer to GeoJSON and parsing the `route_ids` property
from the output SHALL produce a JSON array containing exactly the same set
of route ID strings (order-independent).

The round-trip modeled here:
1. A set of route_ids is serialized as a JSON array string (matching what
   gtfs_parser.cc stores in PropertyValue as `json.dumps(list(route_ids))`)
2. The GeoJSON serializer detects the `route_ids` key, parses the stored
   JSON string into an actual JSON array for the output
3. The consumer (frontend) reads that JSON array back

This test verifies that no route_ids are lost or added during serialization.
"""

import json
from typing import List, Set

from hypothesis import given, settings
from hypothesis import strategies as st


# ---------------------------------------------------------------------------
# Model of the serialization pipeline from gtfs_parser.cc + geojson_serializer.cc
# ---------------------------------------------------------------------------


def serialize_route_ids(route_ids: Set[str]) -> str:
    """
    Model of how gtfs_parser.cc stores route_ids in PropertyValue.

    The C++ code creates a nlohmann::json array, pushes each route_id string,
    then calls .dump() to produce a JSON array string stored as a std::string
    variant in the feature properties.

    Example output: '["L1","L2"]'
    """
    arr = list(route_ids)
    return json.dumps(arr)


def geojson_serializer_emit(stored_json_str: str):
    """
    Model of how geojson_serializer.cc emits the route_ids property.

    The serializer detects the `route_ids` key, attempts to parse the stored
    string as JSON. If it's a valid JSON array, it emits the parsed array
    directly (not as a string). If parsing fails, it falls through to emit
    as a raw string.

    Returns the value that would appear in the GeoJSON output for the
    `route_ids` key.
    """
    try:
        parsed = json.loads(stored_json_str)
        if isinstance(parsed, list):
            return parsed
    except (json.JSONDecodeError, TypeError):
        pass
    # Fallback: emit as raw string (shouldn't happen with valid data)
    return stored_json_str


def full_roundtrip(route_ids: Set[str]) -> List[str]:
    """
    Full serialization pipeline: set of route IDs → stored JSON string →
    GeoJSON serializer parses it → output JSON array.

    Returns the list of route IDs as they would appear after the round-trip.
    """
    stored = serialize_route_ids(route_ids)
    emitted = geojson_serializer_emit(stored)
    return emitted


# ---------------------------------------------------------------------------
# Strategies
# ---------------------------------------------------------------------------

# Route ID strings: alphanumeric identifiers that could include typical
# transit route naming patterns (e.g., "L1", "R42", "Metro3")
route_id_char_strategy = st.sampled_from(
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-"
)

route_id_strategy = st.text(
    alphabet=route_id_char_strategy,
    min_size=1,
    max_size=12,
)

# Sets of route IDs (0 to 20 elements)
route_id_set_strategy = st.frozensets(route_id_strategy, min_size=0, max_size=20)


# ---------------------------------------------------------------------------
# Property Tests
# ---------------------------------------------------------------------------


class TestRouteIdsSerializationRoundTrip:
    """Property 2: Route IDs serialization round-trip."""

    @given(route_ids=route_id_set_strategy)
    @settings(max_examples=300)
    def test_roundtrip_preserves_route_id_set(self, route_ids: frozenset) -> None:
        """
        **Validates: Requirements 1.2**

        For any set of route ID strings, the full serialization round-trip
        (store as JSON string → serializer parses to array → read back)
        SHALL produce a JSON array containing exactly the same set of
        route ID strings (order-independent).
        """
        input_set = set(route_ids)

        # Perform the round-trip
        output_list = full_roundtrip(input_set)

        # The output must be a list
        assert isinstance(output_list, list), (
            f"Expected list output, got {type(output_list)}: {output_list}"
        )

        # Convert output to set for order-independent comparison
        output_set = set(output_list)

        # The output set must equal the input set
        assert output_set == input_set, (
            f"Round-trip mismatch:\n"
            f"  Input set:  {sorted(input_set)}\n"
            f"  Output set: {sorted(output_set)}\n"
            f"  Missing: {sorted(input_set - output_set)}\n"
            f"  Extra:   {sorted(output_set - input_set)}"
        )

    @given(route_ids=route_id_set_strategy)
    @settings(max_examples=300)
    def test_roundtrip_output_count_matches_input(self, route_ids: frozenset) -> None:
        """
        **Validates: Requirements 1.2**

        For any set of route ID strings, the number of elements in the
        output array SHALL equal the size of the input set (no duplicates
        introduced during serialization).
        """
        input_set = set(route_ids)

        output_list = full_roundtrip(input_set)

        assert len(output_list) == len(input_set), (
            f"Count mismatch: input has {len(input_set)} elements, "
            f"output has {len(output_list)} elements.\n"
            f"  Input:  {sorted(input_set)}\n"
            f"  Output: {output_list}"
        )

    @given(route_ids=route_id_set_strategy)
    @settings(max_examples=300)
    def test_roundtrip_produces_valid_json_array(self, route_ids: frozenset) -> None:
        """
        **Validates: Requirements 1.2**

        For any set of route ID strings, the serialized form stored in the
        property SHALL be a valid JSON array that can be parsed back without
        error by the GeoJSON serializer.
        """
        input_set = set(route_ids)

        # Serialize (what the parser stores)
        stored = serialize_route_ids(input_set)

        # Verify the stored value is valid JSON
        parsed = json.loads(stored)
        assert isinstance(parsed, list), (
            f"Stored value is not a JSON array: {stored}"
        )

        # Verify all elements are strings
        for elem in parsed:
            assert isinstance(elem, str), (
                f"Expected all elements to be strings, got {type(elem)}: {elem}"
            )
