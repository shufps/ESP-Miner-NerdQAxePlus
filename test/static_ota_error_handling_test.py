from pathlib import Path
import re
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


class OtaErrorHandlingContractTest(unittest.TestCase):
    def test_direct_ota_handlers_return_errors_instead_of_aborting(self):
        source = _read("main/http_server/handler_ota.cpp")
        self.assertIsNone(re.search(r"\bESP_ERROR_CHECK\s*\(", source))

        www_body = _function_body(source, "esp_err_t POST_WWW_update")
        self.assertIn("esp_partition_erase_range", www_body)
        self.assertRegex(www_body, r"esp_err_t\s+erase_err\s*=\s*esp_partition_erase_range")
        self.assertIn("if (!buf)", www_body)

        fw_body = _function_body(source, "esp_err_t POST_OTA_update")
        self.assertIn("if (ota_partition == NULL)", fw_body)
        self.assertRegex(fw_body, r"esp_err_t\s+ota_err\s*=\s*esp_ota_begin")
        self.assertIn("if (!buf)", fw_body)
        self.assertRegex(fw_body, r"recv_len <= 0\) \{\n\s*esp_ota_abort\(ota_handle\);")
        self.assertRegex(fw_body, r"ota_err\s*=\s*esp_ota_end")
        self.assertRegex(fw_body, r"ota_err\s*=\s*esp_ota_set_boot_partition")

    def test_factory_ota_returns_esp_errors_instead_of_aborting(self):
        source = _read("main/http_server/handler_ota_factory.cpp")
        self.assertIsNone(re.search(r"\bESP_ERROR_CHECK\s*\(", source))

        www_body = _function_body(source, "esp_err_t FactoryOTAUpdate::do_www_update")
        self.assertRegex(www_body, r"esp_err_t\s+err\s*=\s*esp_partition_erase_range")

        fw_body = _function_body(source, "esp_err_t FactoryOTAUpdate::do_firmware_update")
        self.assertIn("if (ota_partition == NULL)", fw_body)
        self.assertRegex(fw_body, r"esp_err_t\s+err\s*=\s*esp_ota_begin")
        self.assertRegex(fw_body, r"err\s*=\s*esp_ota_end")
        self.assertRegex(fw_body, r"err\s*=\s*esp_ota_set_boot_partition")


if __name__ == "__main__":
    unittest.main()
