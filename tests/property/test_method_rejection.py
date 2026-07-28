"""
Feature: gtfs-ui-ingestion, Property 8: Disallowed HTTP method rejection

**Validates: Requirements 7.4**

Property: For any HTTP method that is not POST and not OPTIONS, a request
to /api/ingest/gtfs SHALL receive HTTP status 405.

This test verifies the contract by exercising the actual HttpAdapter server.
The server registers explicit handlers for GET, PUT, DELETE, and PATCH on
/api/ingest/gtfs that return 405 with {"error": "Method not allowed"}.
"""

import json
import subprocess
import time
import socket
import os
import signal

import pytest
import requests
from hypothesis import given, settings, assume, HealthCheck
from hypothesis.strategies import sampled_from


# The disallowed methods that the server explicitly rejects with 405.
DISALLOWED_METHODS = ["GET", "PUT", "DELETE", "PATCH"]

# Port for the test server
TEST_PORT = 18085
BASE_URL = f"http://localhost:{TEST_PORT}/api/ingest/gtfs"


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


def send_request(method, url):
    """Send an HTTP request with the given method."""
    return requests.request(method, url, timeout=5)


@given(method=sampled_from(DISALLOWED_METHODS))
@settings(max_examples=100, suppress_health_check=[HealthCheck.function_scoped_fixture])
def test_disallowed_method_returns_405(server, method):
    """
    Property 8: Disallowed HTTP method rejection.

    For any HTTP method in {GET, PUT, DELETE, PATCH}, a request to
    /api/ingest/gtfs SHALL receive HTTP status 405 with a JSON body
    containing an "error" field.

    Feature: gtfs-ui-ingestion, Property 8: Disallowed HTTP method rejection
    **Validates: Requirements 7.4**
    """
    response = send_request(method, server)

    assert response.status_code == 405, (
        f"Expected 405 for {method} /api/ingest/gtfs, got {response.status_code}"
    )

    # Verify the response body contains the expected error message
    body = response.json()
    assert "error" in body, (
        f"Expected 'error' field in response body for {method}, got: {body}"
    )
    assert body["error"] == "Method not allowed", (
        f"Expected error message 'Method not allowed' for {method}, "
        f"got: {body['error']}"
    )
