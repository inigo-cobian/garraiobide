"""
Feature: gtfs-ui-ingestion, Property 5: Frontend filename extension validation

**Validates: Requirements 6.1, 6.2**

Property 5: For any filename string, the frontend SHALL allow submission if and
only if the filename ends with ".zip" (case-insensitive comparison). For filenames
not matching this pattern, a notification with "Please select a .zip file" SHALL be
displayed and no HTTP request SHALL be sent.

This test re-implements the frontend validation logic (regex /\\.zip$/i) in Python
and uses Hypothesis to verify accept/reject behavior across many random filenames.
"""

import re
import string

from hypothesis import given, settings, assume
from hypothesis import strategies as st


# ---------------------------------------------------------------------------
# System Under Test: re-implementation of frontend filename validation logic
# ---------------------------------------------------------------------------
# Frontend logic from app.js:
#   if (!file.name.match(/\.zip$/i)) {
#       showNotification('Please select a .zip file', 'error');
#       return;
#   }


def validate_filename(filename: str) -> tuple[bool, str | None]:
    """
    Validate a filename using the same logic as the frontend.

    Returns:
        (True, None) if the file is accepted (ends with .zip, case-insensitive).
        (False, error_message) if the file is rejected.
    """
    if re.search(r"\.zip$", filename, re.IGNORECASE):
        return (True, None)
    else:
        return (False, "Please select a .zip file")


# ---------------------------------------------------------------------------
# Reference oracle: pure case-insensitive .zip suffix check
# ---------------------------------------------------------------------------


def ends_with_zip_case_insensitive(filename: str) -> bool:
    """Reference oracle: does the filename end with .zip (case-insensitive)?"""
    return filename.lower().endswith(".zip")


# ---------------------------------------------------------------------------
# Strategies
# ---------------------------------------------------------------------------

# Characters for generating filename bases (printable ASCII without path separators)
_filename_chars = st.sampled_from(
    string.ascii_letters + string.digits + "._-() "
)

# Strategy for filename bases (non-empty strings of filename-safe characters)
_filename_base = st.text(_filename_chars, min_size=1, max_size=50)

# Strategy for .zip in various case combinations
_zip_suffix = st.sampled_from([
    ".zip", ".ZIP", ".Zip", ".zIp", ".ziP", ".ZiP", ".zIP", ".ZIp",
])

# Strategy for non-.zip extensions
_non_zip_extension = st.sampled_from([
    ".txt", ".csv", ".tar.gz", ".rar", ".7z", ".gz", ".bz2",
    ".pdf", ".doc", ".xls", ".json", ".xml", ".html",
    ".zipx", ".zip.bak", "",  # no extension
])


# Strategy: filenames that SHOULD be accepted (end with .zip case-insensitive)
valid_filenames = st.one_of(
    # base + .zip variant
    st.builds(lambda base, suffix: base + suffix, _filename_base, _zip_suffix),
    # Just the suffix alone
    _zip_suffix,
)

# Strategy: filenames that SHOULD be rejected (do NOT end with .zip)
invalid_filenames = st.one_of(
    # base + non-zip extension
    st.builds(lambda base, ext: base + ext, _filename_base, _non_zip_extension),
    # Filenames with .zip in the middle but not at the end
    st.builds(lambda base, ext: base + ".zip" + ext, _filename_base,
              st.sampled_from([".bak", ".old", ".txt", "x", "1"])),
    # Filenames that contain 'zip' but not as a proper extension
    st.builds(lambda base: base + "zip", _filename_base),
)

# Strategy: completely random filenames (mixed valid and invalid)
random_filenames = st.text(
    st.characters(whitelist_categories=("L", "N", "P", "S"),
                  blacklist_characters="\x00/\\"),
    min_size=1,
    max_size=100,
)


# ---------------------------------------------------------------------------
# Property Tests
# ---------------------------------------------------------------------------


@given(filename=valid_filenames)
@settings(max_examples=200)
def test_filenames_ending_with_zip_are_accepted(filename: str):
    """
    Property: Any filename ending with .zip (case-insensitive) SHALL be accepted.
    **Validates: Requirements 6.1**
    """
    accepted, error = validate_filename(filename)
    assert accepted is True, (
        f"Filename '{filename}' should be accepted (ends with .zip) but was rejected"
    )
    assert error is None


@given(filename=invalid_filenames)
@settings(max_examples=200)
def test_filenames_not_ending_with_zip_are_rejected(filename: str):
    """
    Property: Any filename NOT ending with .zip (case-insensitive) SHALL be rejected
    with the error message "Please select a .zip file".
    **Validates: Requirements 6.2**
    """
    # Ensure the generated filename truly doesn't end with .zip
    assume(not ends_with_zip_case_insensitive(filename))

    accepted, error = validate_filename(filename)
    assert accepted is False, (
        f"Filename '{filename}' should be rejected (no .zip suffix) but was accepted"
    )
    assert error == "Please select a .zip file"


@given(filename=random_filenames)
@settings(max_examples=200)
def test_validation_matches_oracle_for_any_filename(filename: str):
    """
    Property: For ANY filename, the validation function's accept/reject decision
    SHALL match the reference oracle (case-insensitive .zip suffix check).
    **Validates: Requirements 6.1, 6.2**
    """
    expected_accepted = ends_with_zip_case_insensitive(filename)
    actual_accepted, error = validate_filename(filename)

    assert actual_accepted == expected_accepted, (
        f"Filename '{filename}': expected accepted={expected_accepted}, "
        f"got accepted={actual_accepted}"
    )

    if not expected_accepted:
        assert error == "Please select a .zip file"
    else:
        assert error is None
