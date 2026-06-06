from pathlib import Path
import unittest

REPO = Path(__file__).resolve().parents[1]


def _read(relative_path: str) -> str:
    return (REPO / relative_path).read_text()


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


class CanPatchBodyContractTest(unittest.TestCase):
    def test_patch_can_slave_uses_shared_json_body_reader(self):
        source = _read("main/http_server/handler_can_swarm.cpp")
        body = _function_body(source, "esp_err_t PATCH_can_slave")

        self.assertIn("getJsonData(req, doc)", body)
        self.assertIn("esp_err_t err = getJsonData(req, doc);", body)
        self.assertIn("if (err != ESP_OK)", body)
        self.assertNotIn("char body[256]", body)
        self.assertNotIn("httpd_req_recv", body)
        self.assertNotIn("deserializeJson(doc, body)", body)


if __name__ == "__main__":
    unittest.main()
