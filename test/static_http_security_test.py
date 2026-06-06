from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def strip_comments(source: str) -> str:
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.S)
    source = re.sub(r"//.*", "", source)
    return source


def function_body(path: str, name: str) -> str:
    text = (ROOT / path).read_text()
    match = re.search(rf"esp_err_t\s+{re.escape(name)}\s*\([^)]*\)\s*{{", text)
    assert match, f"{name} not found in {path}"

    start = text.find("{", match.start())
    depth = 0
    for idx in range(start, len(text)):
        if text[idx] == "{":
            depth += 1
        elif text[idx] == "}":
            depth -= 1
            if depth == 0:
                return strip_comments(text[start : idx + 1])
    raise AssertionError(f"{name} body not closed in {path}")


def assert_has_call(path: str, function: str, call: str) -> None:
    body = function_body(path, function)
    assert call in body, f"{function} in {path} must call {call}"


def test_system_power_actions_require_otp_in_backend() -> None:
    assert_has_call("main/http_server/handler_restart.cpp", "POST_restart", "validateOTP(req")
    assert_has_call("main/http_server/handler_shutdown.cpp", "POST_shutdown", "validateOTP(req")


def test_alert_test_uses_same_network_allow_list_as_alert_update() -> None:
    assert_has_call("main/http_server/handler_alert.cpp", "POST_test_alert", "is_network_allowed(req)")


def test_reset_stats_requires_otp_because_it_mutates_runtime_state() -> None:
    assert_has_call("main/http_server/handler_system.cpp", "POST_reset_stats", "validateOTP(req")
