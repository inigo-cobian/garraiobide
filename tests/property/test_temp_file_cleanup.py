"""
Feature: gtfs-ui-ingestion, Property 2: Temporary file cleanup invariant

**Validates: Requirements 3.3**

Property: For any upload request (whether it results in success, ingestion failure,
persistence failure, or duplicate layer error), the temporary file SHALL not exist
on disk after the handler returns.

The server writes temp files with the pattern `garraiobide_upload_*.zip` in the
system temp directory. This test verifies that no such files remain after the
handler returns, regardless of the payload content or processing outcome.
"""

import glob
import os
import socket
import subprocess
import tempfile
import time

import pytest
import requests
from hypothesis import given, settings, HealthCheck
from hypothesis import strategies as st


# Port for the test server
TEST_PORT = 18086
BASE_URL = f"http://localhost:{TEST_PORT}/api/ingest/gtfs"
TEMP_DIR = tempfile.gettempdir()
TEMP_FILE_PATTERN = os.path.join(TEMP_DIR, "garraiobide_upload_*")


def find_server_binary():
    """Locate the compiled server binary."""
    workspace = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    candidates = [
        os.path.join(workspace, "build", "Release", "src", "garraiobide_app"),
        os.path.join(workspace, "build", "src", "garraiobide_app"),
        os.path.join(workspace, "build", "src", "garraiobide"),
        os.path.join(workspace, "cmake-build-debug", "src", "garraiobide_app"),
        os.path.join(workspace, "cmake-build-debug", "src", "garraiobide"),
    ]
    for path in candidates:
        if os.path.isfile(path) and os.access(path, os.X_OK):
            return path
    return None


def is_port_open(port, host="localhost", timeout=1.0):
    """Check if a port is accepting connections."""
    try:
        with socket.create_connection((host, port), timeout=timeout):
            return True
    except (ConnectionRefusedError, OSError, socket.timeout):
        return False


@pytest.fixture(scope="module")
def server():
    """Start the garraiobide server for testing, or skip if binary not found."""
    binary = find_server_binary()
    if binary is None:
        pytest.skip("Server binary not found. Build the project first.")

    # If port is already in use (server already running), use it directly
    if is_port_open(TEST_PORT):
        yield BASE_URL
        return

    workspace = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    data_dir = os.path.join(workspace, "data")

    proc = subprocess.Popen(
        [binary, "--port", str(TEST_PORT), "--data", data_dir],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )

    # Wait for server to start accepting connections
    for _ in range(50):
        if is_port_open(TEST_PORT):
            break
        time.sleep(0.1)
    else:
        proc.terminate()
        proc.wait()
        pytest.skip("Server did not start within 5 seconds.")

    yield BASE_URL

    proc.terminate()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()


def get_temp_files():
    """Return list of garraiobide temp upload files currently on disk."""
    return glob.glob(TEMP_FILE_PATTERN)


def send_upload(url, content, filename="test_upload.zip"):
    """Send a multipart POST with the given content as the file field."""
    files = {"file": (filename, content, "application/zip")}
    return requests.post(url, files=files, timeout=10)


# Strategy: generate payloads that will exercise different code paths in the handler.
# - Small random bytes (1-1024): will reach temp file stage but fail GTFS parsing (422)
# - Larger random bytes (1025-4096): same outcome, different sizes
# These all get past the "empty file" and "too large" checks and write to temp.
payload_strategy = st.binary(min_size=1, max_size=4096)


@given(payload=payload_strategy)
@settings(
    max_examples=100,
    suppress_health_check=[HealthCheck.function_scoped_fixture],
    deadline=None,
)
def test_temp_file_cleanup_after_invalid_payload(server, payload):
    """
    Property 2: Temporary file cleanup invariant.

    For any non-empty payload that passes size validation and reaches the
    temp file write stage, no garraiobide_upload_* files SHALL remain in
    the temp directory after the handler returns.

    These payloads are random bytes that will fail GTFS ingestion (422),
    exercising the cleanup path on ingestion failure.

    Feature: gtfs-ui-ingestion, Property 2: Temporary file cleanup invariant
    **Validates: Requirements 3.3**
    """
    # Ensure no leftover temp files before the request
    pre_existing = get_temp_files()

    response = send_upload(server, payload)

    # The server should respond (we don't care about the specific status here,
    # only that the handler completed and cleaned up)
    assert response.status_code in (200, 400, 409, 422, 500), (
        f"Unexpected status code: {response.status_code}"
    )

    # The critical property: no temp files remain after the handler returns
    remaining = get_temp_files()
    # Filter out any that existed before our request
    new_temp_files = [f for f in remaining if f not in pre_existing]
    assert new_temp_files == [], (
        f"Temporary upload files were not cleaned up: {new_temp_files}"
    )


@given(payload=st.binary(min_size=0, max_size=0))
@settings(
    max_examples=50,
    suppress_health_check=[HealthCheck.function_scoped_fixture],
    deadline=None,
)
def test_temp_file_cleanup_empty_payload(server, payload):
    """
    Property 2: Temporary file cleanup invariant (empty file path).

    For an empty payload (size=0), the server rejects with 400 before
    writing a temp file. Verify no temp files are left behind.

    Feature: gtfs-ui-ingestion, Property 2: Temporary file cleanup invariant
    **Validates: Requirements 3.3**
    """
    pre_existing = get_temp_files()

    response = send_upload(server, payload)

    assert response.status_code == 400

    remaining = get_temp_files()
    new_temp_files = [f for f in remaining if f not in pre_existing]
    assert new_temp_files == [], (
        f"Temporary upload files were not cleaned up: {new_temp_files}"
    )


@given(
    payload=payload_strategy,
    filename=st.sampled_from([
        "valid_feed.zip",
        "UPPERCASE.ZIP",
        "no_extension",
        "special chars (1).zip",
        "../../traversal.zip",
        "a" * 200 + ".zip",
    ]),
)
@settings(
    max_examples=100,
    suppress_health_check=[HealthCheck.function_scoped_fixture],
    deadline=None,
)
def test_temp_file_cleanup_various_filenames(server, payload, filename):
    """
    Property 2: Temporary file cleanup invariant (various filenames).

    For any combination of payload and filename (including edge-case
    filenames), no temp files SHALL remain after the handler returns.
    The filename affects the derived layer prefix but not the cleanup logic.

    Feature: gtfs-ui-ingestion, Property 2: Temporary file cleanup invariant
    **Validates: Requirements 3.3**
    """
    pre_existing = get_temp_files()

    response = send_upload(server, payload, filename=filename)

    assert response.status_code in (200, 400, 409, 422, 500), (
        f"Unexpected status code: {response.status_code}"
    )

    remaining = get_temp_files()
    new_temp_files = [f for f in remaining if f not in pre_existing]
    assert new_temp_files == [], (
        f"Temporary upload files were not cleaned up: {new_temp_files}"
    )
