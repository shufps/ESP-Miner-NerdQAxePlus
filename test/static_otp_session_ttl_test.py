from pathlib import Path
import re
import unittest

REPO = Path(__file__).resolve().parents[1]


def _read(relative_path: str) -> str:
    return (REPO / relative_path).read_text()


def _function_body(source: str, function_name: str) -> str:
    start = source.index(f"esp_err_t {function_name}")
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
    raise AssertionError(f"Could not find body for {function_name}")


class OtpSessionTtlContractTest(unittest.TestCase):
    def test_backend_honors_requested_otp_session_ttl_header(self):
        source = _read("main/http_server/handler_otp.cpp")
        body = _function_body(source, "POST_create_otp_session")

        self.assertIn("X-OTP-Session-TTL", body)
        self.assertIn("clamp_ttl_ms", body)
        self.assertRegex(body, r"ttlSeconds\s*=\s*ttlMs\s*/\s*1000")
        self.assertIn("otp.mintSessionToken(ttlSeconds)", body)
        self.assertRegex(body, r'doc\["ttlMs"\]\s*=\s*ttlMs\s*;')

    def test_cors_preflight_allows_otp_session_ttl_header(self):
        source = _read("main/http_server/http_cors.cpp")
        self.assertIn("X-OTP-Session-TTL", source)


if __name__ == "__main__":
    unittest.main()
