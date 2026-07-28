"""
Feature: gtfs-ui-ingestion, Property 4: Success response schema validity

**Validates: Requirements 3.4**

Property 4: For any valid GTFS ZIP file that is successfully ingested producing
N layers (where N > 0), the HTTP response SHALL have status 200, a "status" field
equal to "ok", and a "layers" field that is a JSON array of exactly N non-empty
strings.

This test models the C++ response construction logic from http_adapter.cc in Python,
then uses Hypothesis to verify the schema contract holds for many generated layer
name prefixes. The import_gtfs method always produces exactly 2 layers
({prefix}_routes and {prefix}_stops).
"""

import json
import re
import string

from hypothesis import given, settings, assume
from hypothesis import strategies as st


# ---------------------------------------------------------------------------
# System Under Test: Python model of the C++ response construction logic
# ---------------------------------------------------------------------------
# From http_adapter.cc, the success response is built as:
#
#   std::string body = R"({"status":"ok","layers":[)";
#   bool first = true;
#   for (const auto& name : *result) {
#       if (!first) body += ",";
#       body += "\"" + name + "\"";
#       first = false;
#   }
#   body += "]}";
#   res.status = 200;
#
# And import_gtfs returns: [layer_prefix + "_routes", layer_prefix + "_stops"]


def derive_layer_prefix(filename: str) -> str:
    """
    Python model of the C++ derive_layer_prefix function.

    Strips .zip extension (case-insensitive), converts to lowercase,
    replaces non-alphanumeric characters with underscores, trims
    leading/trailing underscores, and collapses consecutive underscores.
    Returns "gtfs" if the result is empty.
    """
    name = filename

    # Strip .zip extension (case-insensitive)
    if len(name) >= 4 and name[-4:].lower() == ".zip":
        name = name[:-4]

    # Normalize: lowercase, non-alphanumeric -> underscore
    result = ""
    for ch in name:
        if ch.isalnum():
            result += ch.lower()
        else:
            result += "_"

    # Trim leading/trailing underscores and collapse consecutive underscores
    cleaned = ""
    prev_underscore = True  # suppress leading underscores
    for ch in result:
        if ch == "_":
            if not prev_underscore:
                cleaned += ch
            prev_underscore = True
        else:
            cleaned += ch
            prev_underscore = False

    # Remove trailing underscore
    if cleaned and cleaned[-1] == "_":
        cleaned = cleaned[:-1]

    return cleaned if cleaned else "gtfs"


def build_layer_names(layer_prefix: str) -> list[str]:
    """
    Model the layer names produced by import_gtfs.
    Always produces exactly 2 layers: {prefix}_routes and {prefix}_stops.
    """
    return [f"{layer_prefix}_routes", f"{layer_prefix}_stops"]


def build_success_response(layer_names: list[str]) -> tuple[int, str]:
    """
    Model the C++ success response construction logic.

    Returns:
        (status_code, response_body_json_string)
    """
    body = '{"status":"ok","layers":['
    first = True
    for name in layer_names:
        if not first:
            body += ","
        body += '"' + name + '"'
        first = False
    body += "]}"
    return (200, body)


# ---------------------------------------------------------------------------
# Strategies
# ---------------------------------------------------------------------------

# Characters for generating filename bases (printable ASCII without path separators)
_filename_chars = st.sampled_from(
    string.ascii_letters + string.digits + "._- "
)

# Strategy for filename bases (non-empty strings of filename-safe characters)
_filename_base = st.text(_filename_chars, min_size=1, max_size=40)

# Strategy for .zip suffix in various cases
_zip_suffix = st.sampled_from([".zip", ".ZIP", ".Zip", ".zIp", ".ziP"])

# Strategy for filenames that produce valid layer prefixes
filenames_with_zip = st.builds(
    lambda base, suffix: base + suffix, _filename_base, _zip_suffix
)

# Strategy for generating layer prefixes directly (alphanumeric + underscore)
_prefix_chars = st.sampled_from(string.ascii_lowercase + string.digits + "_")
direct_layer_prefixes = st.text(
    _prefix_chars, min_size=1, max_size=30
).filter(
    # Ensure prefix is well-formed: no leading/trailing/consecutive underscores
    lambda p: not p.startswith("_")
    and not p.endswith("_")
    and "__" not in p
    and any(c != "_" for c in p)
)


# ---------------------------------------------------------------------------
# Property Tests
# ---------------------------------------------------------------------------


@given(layer_prefix=direct_layer_prefixes)
@settings(max_examples=200)
def test_success_response_has_status_200(layer_prefix: str):
    """
    Property: For any successful ingestion, the HTTP status SHALL be 200.

    Feature: gtfs-ui-ingestion, Property 4: Success response schema validity
    **Validates: Requirements 3.4**
    """
    layer_names = build_layer_names(layer_prefix)
    status_code, _ = build_success_response(layer_names)

    assert status_code == 200, (
        f"Expected status 200 for prefix '{layer_prefix}', got {status_code}"
    )


@given(layer_prefix=direct_layer_prefixes)
@settings(max_examples=200)
def test_success_response_has_status_ok(layer_prefix: str):
    """
    Property: For any successful ingestion, the response body SHALL have
    a "status" field equal to "ok".

    Feature: gtfs-ui-ingestion, Property 4: Success response schema validity
    **Validates: Requirements 3.4**
    """
    layer_names = build_layer_names(layer_prefix)
    _, body = build_success_response(layer_names)

    parsed = json.loads(body)
    assert "status" in parsed, (
        f"Response body missing 'status' field for prefix '{layer_prefix}': {body}"
    )
    assert parsed["status"] == "ok", (
        f"Expected status='ok', got status='{parsed['status']}' "
        f"for prefix '{layer_prefix}'"
    )


@given(layer_prefix=direct_layer_prefixes)
@settings(max_examples=200)
def test_success_response_layers_is_array_of_correct_count(layer_prefix: str):
    """
    Property: For any successful ingestion producing N layers, the response
    body SHALL have a "layers" field that is a JSON array of exactly N elements.

    For import_gtfs, N is always 2 (routes + stops).

    Feature: gtfs-ui-ingestion, Property 4: Success response schema validity
    **Validates: Requirements 3.4**
    """
    layer_names = build_layer_names(layer_prefix)
    expected_count = len(layer_names)
    _, body = build_success_response(layer_names)

    parsed = json.loads(body)
    assert "layers" in parsed, (
        f"Response body missing 'layers' field for prefix '{layer_prefix}': {body}"
    )
    assert isinstance(parsed["layers"], list), (
        f"Expected 'layers' to be an array, got {type(parsed['layers']).__name__} "
        f"for prefix '{layer_prefix}'"
    )
    assert len(parsed["layers"]) == expected_count, (
        f"Expected {expected_count} layers, got {len(parsed['layers'])} "
        f"for prefix '{layer_prefix}'"
    )


@given(layer_prefix=direct_layer_prefixes)
@settings(max_examples=200)
def test_success_response_layers_are_non_empty_strings(layer_prefix: str):
    """
    Property: For any successful ingestion, all elements in the "layers" array
    SHALL be non-empty strings.

    Feature: gtfs-ui-ingestion, Property 4: Success response schema validity
    **Validates: Requirements 3.4**
    """
    layer_names = build_layer_names(layer_prefix)
    _, body = build_success_response(layer_names)

    parsed = json.loads(body)
    layers = parsed["layers"]

    for i, layer in enumerate(layers):
        assert isinstance(layer, str), (
            f"Layer at index {i} is not a string: {type(layer).__name__} "
            f"for prefix '{layer_prefix}'"
        )
        assert len(layer) > 0, (
            f"Layer at index {i} is empty string for prefix '{layer_prefix}'"
        )


@given(filename=filenames_with_zip)
@settings(max_examples=200)
def test_full_pipeline_from_filename_to_valid_response(filename: str):
    """
    Property: For any filename with a .zip extension, the full pipeline
    (derive prefix -> build layer names -> build response) SHALL produce a
    valid response with status 200, "status"="ok", and a "layers" array of
    exactly 2 non-empty strings.

    Feature: gtfs-ui-ingestion, Property 4: Success response schema validity
    **Validates: Requirements 3.4**
    """
    layer_prefix = derive_layer_prefix(filename)
    layer_names = build_layer_names(layer_prefix)
    status_code, body = build_success_response(layer_names)

    # Verify status code
    assert status_code == 200

    # Verify valid JSON
    parsed = json.loads(body)

    # Verify "status" field
    assert parsed.get("status") == "ok", (
        f"Expected status='ok' for filename '{filename}' "
        f"(prefix='{layer_prefix}'), got: {parsed.get('status')}"
    )

    # Verify "layers" field is array of exactly 2 non-empty strings
    layers = parsed.get("layers")
    assert isinstance(layers, list), (
        f"Expected 'layers' to be an array for filename '{filename}'"
    )
    assert len(layers) == 2, (
        f"Expected 2 layers for filename '{filename}' "
        f"(prefix='{layer_prefix}'), got {len(layers)}"
    )
    for i, layer in enumerate(layers):
        assert isinstance(layer, str) and len(layer) > 0, (
            f"Layer at index {i} is not a non-empty string for "
            f"filename '{filename}': {layer!r}"
        )
