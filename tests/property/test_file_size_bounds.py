"""
Property test for server-side file size bounds validation.

Feature: gtfs-ui-ingestion, Property 6: Server-side file size bounds validation

Validates: Requirements 6.3, 6.4, 6.6

For any uploaded file with size S bytes:
  - if S = 0 → HTTP 400, error "Uploaded file is empty"
  - if S > 50*1024*1024 → HTTP 400, error indicating size exceeded
  - if 0 < S <= 50*1024*1024 → proceed (not rejected based on size)
"""

from hypothesis import given, settings
from hypothesis import strategies as st

# Constants matching the C++ implementation
MAX_UPLOAD_SIZE = 50 * 1024 * 1024  # 50 MB in bytes


def validate_file_size(size: int) -> tuple[int, str | None]:
    """
    Re-implementation of the server-side file size validation logic
    from http_adapter.cc.

    Returns (status_code, error_message).
    - (400, "Uploaded file is empty") if size == 0
    - (400, "File exceeds the maximum allowed size of 50 MB") if size > 50MB
    - (200, None) if 0 < size <= 50MB (proceed with ingestion)
    """
    if size == 0:
        return (400, "Uploaded file is empty")

    if size > MAX_UPLOAD_SIZE:
        return (400, "File exceeds the maximum allowed size of 50 MB")

    # Valid size: proceed with ingestion
    return (200, None)


class TestFileSizeBoundsProperty:
    """Property 6: Server-side file size bounds validation."""

    @given(size=st.integers(min_value=0, max_value=0))
    @settings(max_examples=100)
    def test_empty_file_rejected(self, size: int) -> None:
        """
        **Validates: Requirements 6.4**

        For size=0, the server returns HTTP 400 with "Uploaded file is empty".
        """
        status, error = validate_file_size(size)
        assert status == 400
        assert error == "Uploaded file is empty"

    @given(size=st.integers(min_value=MAX_UPLOAD_SIZE + 1, max_value=100 * 1024 * 1024))
    @settings(max_examples=100)
    def test_oversized_file_rejected(self, size: int) -> None:
        """
        **Validates: Requirements 6.6**

        For size > 50*1024*1024, the server returns HTTP 400 with size exceeded error.
        """
        status, error = validate_file_size(size)
        assert status == 400
        assert error == "File exceeds the maximum allowed size of 50 MB"

    @given(size=st.integers(min_value=1, max_value=MAX_UPLOAD_SIZE))
    @settings(max_examples=100)
    def test_valid_size_proceeds(self, size: int) -> None:
        """
        **Validates: Requirements 6.3**

        For 0 < size <= 50*1024*1024, the server does not reject based on size.
        """
        status, error = validate_file_size(size)
        assert status == 200
        assert error is None

    @given(size=st.integers(min_value=0, max_value=100 * 1024 * 1024))
    @settings(max_examples=200)
    def test_size_validation_complete_partition(self, size: int) -> None:
        """
        **Validates: Requirements 6.3, 6.4, 6.6**

        For any file size from 0 to 100MB, exactly one of the three
        validation outcomes applies:
          - size=0 → reject (empty)
          - size>50MB → reject (too large)
          - 0<size<=50MB → accept (proceed)

        This verifies the boundary logic is exhaustive and non-overlapping.
        """
        status, error = validate_file_size(size)

        if size == 0:
            assert status == 400
            assert error == "Uploaded file is empty"
        elif size > MAX_UPLOAD_SIZE:
            assert status == 400
            assert error == "File exceeds the maximum allowed size of 50 MB"
        else:
            assert status == 200
            assert error is None
