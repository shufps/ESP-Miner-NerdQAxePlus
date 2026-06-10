from pathlib import Path


REPO = Path(__file__).resolve().parents[1]


def _function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    brace_start = source.index("{", start)
    depth = 0
    for index in range(brace_start, len(source)):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[brace_start:index + 1]
    raise AssertionError(f"Could not find body for {signature}")


def test_wifi_scan_timeout_resumes_sta_reconnect_attempts() -> None:
    source = (REPO / "main/network/connect.cpp").read_text()
    body = _function_body(source, "esp_err_t wifi_scan")

    timeout_log = body.index('ESP_LOGE(TAG, "WiFi scan timeout")')
    timeout_return = body.index("return ESP_ERR_TIMEOUT", timeout_log)
    timeout_branch = body[timeout_log:timeout_return]

    assert "s_is_scanning = false;" in timeout_branch
    assert "s_scan_suppress_reconnect = false;" in timeout_branch
    assert "if (s_has_ssid) esp_wifi_connect();" in timeout_branch
