from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def test_cors_allows_frontend_auth_headers() -> None:
    cors = (ROOT / "main/http_server/http_cors.cpp").read_text()
    match = re.search(
        r'Access-Control-Allow-Headers"\s*,\s*"([^"]+)"',
        cors,
    )
    assert match, "Access-Control-Allow-Headers not found"
    allowed = {part.strip().lower() for part in match.group(1).split(',')}

    assert "content-type" in allowed
    assert "authorization" in allowed
    assert "x-totp" in allowed
    assert "x-otp-session" in allowed


def test_otp_session_interceptor_only_attaches_to_same_origin_requests() -> None:
    interceptor = (
        ROOT
        / "main/http_server/axe-os/src/app/services/otp-session.interceptor.ts"
    ).read_text()

    assert "shouldAttachOtpSession" in interceptor
    assert "window.location.origin" in interceptor
    assert "new URL" in interceptor
    assert "X-OTP-Session" in interceptor

    # The header clone must be guarded by the same-origin predicate, not just by token expiry.
    predicate_pos = interceptor.find("shouldAttachOtpSession")
    header_pos = interceptor.find("X-OTP-Session")
    assert predicate_pos != -1 and header_pos != -1 and predicate_pos < header_pos
