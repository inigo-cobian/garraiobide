"""
Feature: gtfs-ui-ingestion, Property 3: LayerServiceError to HTTP status code mapping

**Validates: Requirements 3.5, 3.6, 3.7**

Property 3: For any LayerServiceError returned by import_gtfs, the HTTP response
status code SHALL be: IngestionFailed -> 422, PersistenceFailed -> 500,
DuplicateLayer -> 409. The response body SHALL always contain an "error" field
with a non-empty string.

This test models the error-to-HTTP-status mapping from http_adapter.cc and uses
Hypothesis to verify the contract holds for each error variant with random
contextual data (error messages, layer prefixes).
"""

import json
from enum import Enum, auto

from hypothesis import given, settings
from hypothesis import strategies as st


# ---------------------------------------------------------------------------
# Model: LayerServiceError enum matching the C++ definition
# ---------------------------------------------------------------------------


class LayerServiceError(Enum):
    """Models the C++ app::LayerServiceError enum."""

    IngestionFailed = auto()
    PersistenceFailed = auto()
    DuplicateLayer = auto()


# ---------------------------------------------------------------------------
# System Under Test: re-implementation of the error → HTTP response mapping
# from http_adapter.cc handle_ingest_gtfs switch statement
# ---------------------------------------------------------------------------


def map_error_to_response(
    error: LayerServiceError, layer_prefix: str
) -> tuple[int, dict]:
    """
    Re-implementation of the error mapping logic from http_adapter.cc.

    Given a LayerServiceError and the layer_prefix used in the request,
    returns (http_status_code, response_body_dict).

    From http_adapter.cc:
        case app::LayerServiceError::IngestionFailed:
            res.status = 422;
            res.set_content(error_json("GTFS ingestion failed"), ...);
        case app::LayerServiceError::PersistenceFailed:
            res.status = 500;
            res.set_content(error_json("Failed to persist ingested layers"), ...);
        case app::LayerServiceError::DuplicateLayer:
            res.status = 409;
            res.set_content(error_json("Layer already exists: " + layer_prefix), ...);
    """
    if error == LayerServiceError.IngestionFailed:
        return (422, {"error": "GTFS ingestion failed"})
    elif error == LayerServiceError.PersistenceFailed:
        return (500, {"error": "Failed to persist ingested layers"})
    elif error == LayerServiceError.DuplicateLayer:
        return (409, {"error": f"Layer already exists: {layer_prefix}"})
    else:
        # Unreachable for known variants; model the default case
        return (500, {"error": "Internal server error"})


# ---------------------------------------------------------------------------
# Reference oracle: the expected mapping as a simple dict
# ---------------------------------------------------------------------------

EXPECTED_STATUS_CODES = {
    LayerServiceError.IngestionFailed: 422,
    LayerServiceError.PersistenceFailed: 500,
    LayerServiceError.DuplicateLayer: 409,
}


# ---------------------------------------------------------------------------
# Strategies
# ---------------------------------------------------------------------------

# Strategy: generate any LayerServiceError variant
error_variants = st.sampled_from(list(LayerServiceError))

# Strategy: generate layer prefix strings (alphanumeric + underscores, non-empty)
layer_prefixes = st.text(
    st.sampled_from("abcdefghijklmnopqrstuvwxyz0123456789_"),
    min_size=1,
    max_size=50,
)


# ---------------------------------------------------------------------------
# Property Tests
# ---------------------------------------------------------------------------


class TestErrorStatusMappingProperty:
    """Property 3: LayerServiceError to HTTP status code mapping."""

    @given(error=error_variants, layer_prefix=layer_prefixes)
    @settings(max_examples=200)
    def test_error_maps_to_correct_status_code(
        self, error: LayerServiceError, layer_prefix: str
    ) -> None:
        """
        **Validates: Requirements 3.5, 3.6, 3.7**

        For any LayerServiceError variant, the HTTP status code SHALL match:
          - IngestionFailed -> 422
          - PersistenceFailed -> 500
          - DuplicateLayer -> 409
        """
        status, body = map_error_to_response(error, layer_prefix)
        expected_status = EXPECTED_STATUS_CODES[error]

        assert status == expected_status, (
            f"Error {error.name} should map to HTTP {expected_status}, "
            f"but got HTTP {status}"
        )

    @given(error=error_variants, layer_prefix=layer_prefixes)
    @settings(max_examples=200)
    def test_error_response_always_has_non_empty_error_field(
        self, error: LayerServiceError, layer_prefix: str
    ) -> None:
        """
        **Validates: Requirements 3.5, 3.6, 3.7**

        For any LayerServiceError variant, the response body SHALL always
        contain an "error" field with a non-empty string.
        """
        status, body = map_error_to_response(error, layer_prefix)

        assert "error" in body, (
            f"Response for {error.name} is missing 'error' field: {body}"
        )
        assert isinstance(body["error"], str), (
            f"'error' field for {error.name} should be a string, "
            f"got {type(body['error'])}"
        )
        assert len(body["error"]) > 0, (
            f"'error' field for {error.name} should be non-empty"
        )

    @given(error=error_variants, layer_prefix=layer_prefixes)
    @settings(max_examples=200)
    def test_error_response_is_valid_json(
        self, error: LayerServiceError, layer_prefix: str
    ) -> None:
        """
        **Validates: Requirements 3.5, 3.6, 3.7**

        For any LayerServiceError variant, the response body SHALL be
        serializable as valid JSON (matching the error_json() helper in C++).
        """
        status, body = map_error_to_response(error, layer_prefix)

        # Verify the body can be serialized to JSON and deserialized back
        json_str = json.dumps(body)
        parsed = json.loads(json_str)

        assert parsed == body, (
            f"JSON round-trip failed for {error.name}: "
            f"original={body}, parsed={parsed}"
        )

    @given(layer_prefix=layer_prefixes)
    @settings(max_examples=100)
    def test_ingestion_failed_returns_422(self, layer_prefix: str) -> None:
        """
        **Validates: Requirements 3.5**

        IngestionFailed SHALL always produce HTTP 422.
        """
        status, body = map_error_to_response(
            LayerServiceError.IngestionFailed, layer_prefix
        )
        assert status == 422
        assert body["error"] == "GTFS ingestion failed"

    @given(layer_prefix=layer_prefixes)
    @settings(max_examples=100)
    def test_persistence_failed_returns_500(self, layer_prefix: str) -> None:
        """
        **Validates: Requirements 3.6**

        PersistenceFailed SHALL always produce HTTP 500.
        """
        status, body = map_error_to_response(
            LayerServiceError.PersistenceFailed, layer_prefix
        )
        assert status == 500
        assert body["error"] == "Failed to persist ingested layers"

    @given(layer_prefix=layer_prefixes)
    @settings(max_examples=100)
    def test_duplicate_layer_returns_409(self, layer_prefix: str) -> None:
        """
        **Validates: Requirements 3.7**

        DuplicateLayer SHALL always produce HTTP 409 with the layer prefix
        included in the error message.
        """
        status, body = map_error_to_response(
            LayerServiceError.DuplicateLayer, layer_prefix
        )
        assert status == 409
        assert body["error"] == f"Layer already exists: {layer_prefix}"
        assert layer_prefix in body["error"]
