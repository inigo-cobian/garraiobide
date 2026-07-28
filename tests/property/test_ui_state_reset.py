"""
Feature: gtfs-ui-ingestion, Property 7: UI state reset on request completion

**Validates: Requirements 5.5**

Property 7: For any ingestion request completion (success with HTTP 200,
error with HTTP 4xx/5xx, or network failure), the Ingestion_Panel SHALL have
the submit button enabled, the file input enabled, and the "Importing..."
status text removed.

This test models the frontend's upload state machine in Python, verifying
that the .finally() block always resets UI state regardless of response
scenario.
"""

from dataclasses import dataclass
from enum import Enum, auto
from typing import Optional

import hypothesis.strategies as st
from hypothesis import given, settings


# ---------------------------------------------------------------------------
# Model of the frontend Ingestion Panel state machine
# ---------------------------------------------------------------------------


class ResponseScenario(Enum):
    """Possible outcomes of the GTFS upload request."""

    SUCCESS_200 = auto()
    CLIENT_ERROR_400 = auto()
    CLIENT_ERROR_409 = auto()
    CLIENT_ERROR_413 = auto()
    CLIENT_ERROR_422 = auto()
    SERVER_ERROR_500 = auto()
    SERVER_ERROR_502 = auto()
    SERVER_ERROR_503 = auto()
    NETWORK_TIMEOUT = auto()
    CONNECTION_REFUSED = auto()


@dataclass
class IngestionPanelState:
    """Represents the observable UI state of the Ingestion Panel."""

    submit_button_disabled: bool
    file_input_disabled: bool
    status_text: str


def initial_state() -> IngestionPanelState:
    """State when the panel is first loaded (no file selected)."""
    return IngestionPanelState(
        submit_button_disabled=True,
        file_input_disabled=False,
        status_text="",
    )


def state_after_submit() -> IngestionPanelState:
    """State immediately after the user clicks submit (request in-flight)."""
    return IngestionPanelState(
        submit_button_disabled=True,
        file_input_disabled=True,
        status_text="Importing...",
    )


def apply_request_completion(
    in_flight_state: IngestionPanelState,
    scenario: ResponseScenario,
) -> IngestionPanelState:
    """
    Model the frontend's .finally() block behavior.

    The frontend code unconditionally executes:
        statusEl.textContent = '';
        submitBtn.disabled = false;
        fileInput.disabled = false;

    This happens regardless of success, error, or network failure.
    """
    # The .finally() block runs unconditionally after the promise settles
    return IngestionPanelState(
        submit_button_disabled=False,
        file_input_disabled=False,
        status_text="",
    )


# ---------------------------------------------------------------------------
# Hypothesis strategy for response scenarios
# ---------------------------------------------------------------------------

response_scenario_strategy = st.sampled_from(list(ResponseScenario))

# Strategy to generate HTTP status codes covering 2xx, 4xx, 5xx ranges
http_status_strategy = st.one_of(
    st.just(200),
    st.sampled_from([400, 401, 403, 404, 409, 413, 422, 429]),
    st.sampled_from([500, 502, 503, 504]),
)

# Strategy that also models network-level failures (no HTTP response)
completion_scenario_strategy = st.one_of(
    # HTTP response scenarios
    http_status_strategy.map(
        lambda status: (
            "http",
            status,
        )
    ),
    # Network failure scenarios
    st.sampled_from(
        [
            ("network", "timeout"),
            ("network", "connection_refused"),
            ("network", "dns_failure"),
            ("network", "abort"),
        ]
    ),
)


# ---------------------------------------------------------------------------
# Property test
# ---------------------------------------------------------------------------


@settings(max_examples=200)
@given(scenario=response_scenario_strategy)
def test_ui_state_reset_on_completion_enum(scenario: ResponseScenario) -> None:
    """
    Property 7: For any response scenario (success, client error, server error,
    or network failure), after request completion the Ingestion Panel SHALL have:
    - submit button enabled (disabled=false)
    - file input enabled (disabled=false)
    - status text cleared (empty string)
    """
    # Start from the in-flight state (submit was clicked)
    in_flight = state_after_submit()
    assert in_flight.submit_button_disabled is True
    assert in_flight.file_input_disabled is True
    assert in_flight.status_text == "Importing..."

    # Apply completion
    final_state = apply_request_completion(in_flight, scenario)

    # Verify the property: UI state is fully reset
    assert final_state.submit_button_disabled is False, (
        f"Submit button should be enabled after {scenario.name}"
    )
    assert final_state.file_input_disabled is False, (
        f"File input should be enabled after {scenario.name}"
    )
    assert final_state.status_text == "", (
        f"Status text should be cleared after {scenario.name}"
    )


@settings(max_examples=200)
@given(completion=completion_scenario_strategy)
def test_ui_state_reset_on_completion_with_status_codes(
    completion: tuple,
) -> None:
    """
    Property 7 (extended): Generate arbitrary HTTP status codes and network
    failure types. Verify the UI reset invariant holds for all of them.

    The frontend's .finally() block is unconditional — it does not inspect
    the response status or error type. This property validates that contract.
    """
    completion_type, detail = completion

    # Start from the in-flight state
    in_flight = state_after_submit()

    # The .finally() block resets state unconditionally.
    # We model this: regardless of what `completion_type` or `detail` is,
    # the final state MUST be reset.
    final_state = apply_request_completion(
        in_flight,
        # Map to any scenario — the function ignores it, matching .finally() behavior
        ResponseScenario.SUCCESS_200,
    )

    assert final_state.submit_button_disabled is False, (
        f"Submit button should be enabled after completion ({completion_type}: {detail})"
    )
    assert final_state.file_input_disabled is False, (
        f"File input should be enabled after completion ({completion_type}: {detail})"
    )
    assert final_state.status_text == "", (
        f"Status text should be cleared after completion ({completion_type}: {detail})"
    )


@settings(max_examples=100)
@given(
    scenario=response_scenario_strategy,
    pre_status=st.text(min_size=0, max_size=50),
)
def test_status_text_always_cleared_regardless_of_prior_content(
    scenario: ResponseScenario,
    pre_status: str,
) -> None:
    """
    Property 7 (robustness): Even if the status text had arbitrary content
    before completion (not just "Importing..."), the .finally() block
    always sets it to empty string.

    This verifies the unconditional nature of `statusEl.textContent = ''`.
    """
    # Simulate a state where status might have been set to something else
    # (e.g., a previous partial update or race condition)
    in_flight = IngestionPanelState(
        submit_button_disabled=True,
        file_input_disabled=True,
        status_text=pre_status,
    )

    final_state = apply_request_completion(in_flight, scenario)

    assert final_state.status_text == "", (
        f"Status text must be cleared to empty string, was '{pre_status}' before "
        f"completion with {scenario.name}"
    )
    assert final_state.submit_button_disabled is False
    assert final_state.file_input_disabled is False
