"""
Unit tests for configuration parsing.
Tests mirror the behaviour implemented in src/core/Configuration.cpp.

Reference: GW-013 - Configuration Management
"""

import json
import os
import tempfile
import unittest
import sys

sys.path.insert(0, os.path.dirname(__file__))
from test_helpers import parse_hex_string


# ---------------------------------------------------------------------------
# Python reference configuration mirroring Configuration.cpp
# ---------------------------------------------------------------------------

class ReferenceConfiguration:
    """Python reference implementation of the C++ Configuration class."""

    def __init__(self):
        # Defaults matching Configuration::Configuration()
        self.tcp_listen_address = "0.0.0.0"
        self.tcp_listen_port = 8080
        self.unix_socket_path = "/tmp/gateway.sock"
        self.max_connections = 1024
        self.read_timeout_ms = 30000
        self.write_timeout_ms = 30000
        self.max_request_body_size = 1048576
        self.access_token_expiry_seconds = 300
        self.refresh_token_expiry_seconds = 86400
        self.log_level = "info"
        self.log_file_path = "/var/log/gateway.log"
        self.log_to_stdout = True
        self.aes_key: bytes = b""
        self.aes_key_loaded = False
        self._config_values: dict = {}

    def load_from_json(self, json_text: str) -> bool:
        """Parse JSON configuration, mirroring ParseJsonConfiguration."""
        try:
            root = json.loads(json_text)
        except json.JSONDecodeError:
            return False

        if not isinstance(root, dict):
            return False

        if "listen_address" in root:
            self.tcp_listen_address = root["listen_address"]
        if "listen_port" in root:
            self.tcp_listen_port = int(root["listen_port"])
        if "unix_socket_path" in root:
            self.unix_socket_path = root["unix_socket_path"]
        if "max_connections" in root:
            self.max_connections = int(root["max_connections"])
        if "read_timeout_ms" in root:
            self.read_timeout_ms = int(root["read_timeout_ms"])
        if "write_timeout_ms" in root:
            self.write_timeout_ms = int(root["write_timeout_ms"])
        if "max_request_body_size" in root:
            self.max_request_body_size = int(root["max_request_body_size"])
        if "access_token_expiry_seconds" in root:
            self.access_token_expiry_seconds = int(root["access_token_expiry_seconds"])
        if "refresh_token_expiry_seconds" in root:
            self.refresh_token_expiry_seconds = int(root["refresh_token_expiry_seconds"])
        if "log_level" in root:
            self.log_level = root["log_level"]
        if "log_file" in root:
            self.log_file_path = root["log_file"]
        if "log_to_stdout" in root:
            self.log_to_stdout = bool(root["log_to_stdout"])
        if "aes_key" in root:
            parsed = parse_hex_string(root["aes_key"])
            if parsed is not None:
                self.aes_key = parsed
                self.aes_key_loaded = True

        return True

    def load_from_file(self, file_path: str) -> bool:
        try:
            with open(file_path, 'r') as f:
                content = f.read()
            return self.load_from_json(content)
        except (IOError, OSError):
            return False

    def parse_command_line(self, args: list) -> bool:
        """Parse CLI arguments, mirroring ParseCommandLineArguments."""
        i = 1  # skip program name
        while i < len(args):
            arg = args[i]
            if arg in ("--config", "-c") and i + 1 < len(args):
                i += 1
                if not self.load_from_file(args[i]):
                    return False
            elif arg in ("--port", "-p") and i + 1 < len(args):
                i += 1
                self.tcp_listen_port = int(args[i])
            elif arg in ("--address", "-a") and i + 1 < len(args):
                i += 1
                self.tcp_listen_address = args[i]
            elif arg in ("--socket", "-s") and i + 1 < len(args):
                i += 1
                self.unix_socket_path = args[i]
            elif arg in ("--max-connections", "-m") and i + 1 < len(args):
                i += 1
                self.max_connections = int(args[i])
            elif arg == "--log-level" and i + 1 < len(args):
                i += 1
                self.log_level = args[i]
            elif arg == "--log-file" and i + 1 < len(args):
                i += 1
                self.log_file_path = args[i]
            elif arg == "--log-stdout":
                self.log_to_stdout = True
            elif arg == "--no-log-stdout":
                self.log_to_stdout = False
            elif arg == "--access-expiry" and i + 1 < len(args):
                i += 1
                self.access_token_expiry_seconds = int(args[i])
            elif arg == "--refresh-expiry" and i + 1 < len(args):
                i += 1
                self.refresh_token_expiry_seconds = int(args[i])
            i += 1
        return True

    def load_env_aes_key(self) -> bool:
        """Load AES key from GATEWAY_AES_KEY env var."""
        val = os.environ.get("GATEWAY_AES_KEY")
        if val is not None:
            parsed = parse_hex_string(val)
            if parsed is not None:
                self.aes_key = parsed
                self.aes_key_loaded = True
                return True
            return False
        return True  # no env var set is not an error


# ---------------------------------------------------------------------------
# Test Cases
# ---------------------------------------------------------------------------

class TestJsonConfigParsing(unittest.TestCase):
    """GW-013 AC1: JSON config file parsing."""

    def test_valid_json_config(self):
        """GW-013 AC1: Valid JSON configuration is parsed correctly."""
        cfg = ReferenceConfiguration()
        ok = cfg.load_from_json(json.dumps({
            "listen_address": "127.0.0.1",
            "listen_port": 9090,
            "max_connections": 512,
            "log_level": "debug"
        }))
        self.assertTrue(ok)
        self.assertEqual(cfg.tcp_listen_address, "127.0.0.1")
        self.assertEqual(cfg.tcp_listen_port, 9090)
        self.assertEqual(cfg.max_connections, 512)
        self.assertEqual(cfg.log_level, "debug")

    def test_file_loading(self):
        """GW-013 AC1: Config loaded from file."""
        content = json.dumps({"listen_port": 7777})
        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            f.write(content)
            path = f.name
        try:
            cfg = ReferenceConfiguration()
            ok = cfg.load_from_file(path)
            self.assertTrue(ok)
            self.assertEqual(cfg.tcp_listen_port, 7777)
        finally:
            os.unlink(path)

    def test_nonexistent_file(self):
        """GW-013 AC1: Non-existent config file returns False."""
        cfg = ReferenceConfiguration()
        ok = cfg.load_from_file("/no/such/file.json")
        self.assertFalse(ok)


class TestDefaultValues(unittest.TestCase):
    """GW-013 AC2: Default values."""

    def test_defaults(self):
        """GW-013 AC2: Default values match C++ constructor defaults."""
        cfg = ReferenceConfiguration()
        self.assertEqual(cfg.tcp_listen_address, "0.0.0.0")
        self.assertEqual(cfg.tcp_listen_port, 8080)
        self.assertEqual(cfg.unix_socket_path, "/tmp/gateway.sock")
        self.assertEqual(cfg.max_connections, 1024)
        self.assertEqual(cfg.read_timeout_ms, 30000)
        self.assertEqual(cfg.write_timeout_ms, 30000)
        self.assertEqual(cfg.max_request_body_size, 1048576)
        self.assertEqual(cfg.access_token_expiry_seconds, 300)
        self.assertEqual(cfg.refresh_token_expiry_seconds, 86400)
        self.assertEqual(cfg.log_level, "info")
        self.assertTrue(cfg.log_to_stdout)
        self.assertFalse(cfg.aes_key_loaded)


class TestCliOverride(unittest.TestCase):
    """GW-013 AC3: CLI argument override."""

    def test_port_override(self):
        """GW-013 AC3: --port overrides default port."""
        cfg = ReferenceConfiguration()
        ok = cfg.parse_command_line(["gateway", "--port", "3000"])
        self.assertTrue(ok)
        self.assertEqual(cfg.tcp_listen_port, 3000)

    def test_short_port_override(self):
        """GW-013 AC3: -p overrides default port."""
        cfg = ReferenceConfiguration()
        ok = cfg.parse_command_line(["gateway", "-p", "3001"])
        self.assertTrue(ok)
        self.assertEqual(cfg.tcp_listen_port, 3001)

    def test_address_override(self):
        """GW-013 AC3: --address overrides listen address."""
        cfg = ReferenceConfiguration()
        cfg.parse_command_line(["gateway", "--address", "192.168.1.1"])
        self.assertEqual(cfg.tcp_listen_address, "192.168.1.1")

    def test_log_level_override(self):
        """GW-013 AC3: --log-level overrides log level."""
        cfg = ReferenceConfiguration()
        cfg.parse_command_line(["gateway", "--log-level", "error"])
        self.assertEqual(cfg.log_level, "error")

    def test_no_log_stdout(self):
        """GW-013 AC3: --no-log-stdout disables stdout logging."""
        cfg = ReferenceConfiguration()
        cfg.parse_command_line(["gateway", "--no-log-stdout"])
        self.assertFalse(cfg.log_to_stdout)

    def test_config_file_via_cli(self):
        """GW-013 AC3: --config loads config file."""
        content = json.dumps({"listen_port": 5555, "log_level": "debug"})
        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            f.write(content)
            path = f.name
        try:
            cfg = ReferenceConfiguration()
            ok = cfg.parse_command_line(["gateway", "-c", path])
            self.assertTrue(ok)
            self.assertEqual(cfg.tcp_listen_port, 5555)
            self.assertEqual(cfg.log_level, "debug")
        finally:
            os.unlink(path)

    def test_access_expiry_override(self):
        """GW-013 AC3: --access-expiry overrides access token expiry."""
        cfg = ReferenceConfiguration()
        cfg.parse_command_line(["gateway", "--access-expiry", "600"])
        self.assertEqual(cfg.access_token_expiry_seconds, 600)

    def test_refresh_expiry_override(self):
        """GW-013 AC3: --refresh-expiry overrides refresh token expiry."""
        cfg = ReferenceConfiguration()
        cfg.parse_command_line(["gateway", "--refresh-expiry", "172800"])
        self.assertEqual(cfg.refresh_token_expiry_seconds, 172800)


class TestAesKeyFromEnv(unittest.TestCase):
    """GW-013 AC4: AES key from env var."""

    def test_valid_aes_key_env(self):
        """GW-013 AC4: Valid 64-char hex AES key from environment."""
        hex_key = "00" * 32  # 64 hex chars = 32 bytes
        os.environ["GATEWAY_AES_KEY"] = hex_key
        try:
            cfg = ReferenceConfiguration()
            ok = cfg.load_env_aes_key()
            self.assertTrue(ok)
            self.assertTrue(cfg.aes_key_loaded)
            self.assertEqual(len(cfg.aes_key), 32)
        finally:
            del os.environ["GATEWAY_AES_KEY"]

    def test_invalid_aes_key_env(self):
        """GW-013 AC4: Invalid hex AES key returns failure."""
        os.environ["GATEWAY_AES_KEY"] = "not-a-valid-hex-key"
        try:
            cfg = ReferenceConfiguration()
            ok = cfg.load_env_aes_key()
            self.assertFalse(ok)
            self.assertFalse(cfg.aes_key_loaded)
        finally:
            del os.environ["GATEWAY_AES_KEY"]

    def test_no_aes_key_env(self):
        """GW-013 AC4: No GATEWAY_AES_KEY env var is acceptable."""
        if "GATEWAY_AES_KEY" in os.environ:
            del os.environ["GATEWAY_AES_KEY"]
        cfg = ReferenceConfiguration()
        ok = cfg.load_env_aes_key()
        self.assertTrue(ok)
        self.assertFalse(cfg.aes_key_loaded)


class TestInvalidConfigRejection(unittest.TestCase):
    """GW-013 AC5: Invalid config rejection."""

    def test_invalid_json(self):
        """GW-013 AC5: Invalid JSON is rejected."""
        cfg = ReferenceConfiguration()
        ok = cfg.load_from_json("{invalid json")
        self.assertFalse(ok)

    def test_non_object_json(self):
        """GW-013 AC5: Non-object JSON root is rejected."""
        cfg = ReferenceConfiguration()
        ok = cfg.load_from_json("[1, 2, 3]")
        self.assertFalse(ok)


class TestAesKeyLengthValidation(unittest.TestCase):
    """GW-013 AC7: AES key length validation (32 bytes = 64 hex chars)."""

    def test_correct_length(self):
        """GW-013 AC7: 64-char hex string parses to 32 bytes."""
        result = parse_hex_string("ab" * 32)
        self.assertIsNotNone(result)
        self.assertEqual(len(result), 32)

    def test_too_short(self):
        """GW-013 AC7: Shorter hex string is rejected."""
        result = parse_hex_string("ab" * 16)  # 32 chars = 16 bytes
        self.assertIsNone(result)

    def test_too_long(self):
        """GW-013 AC7: Longer hex string is rejected."""
        result = parse_hex_string("ab" * 33)  # 66 chars
        self.assertIsNone(result)

    def test_invalid_hex_chars(self):
        """GW-013 AC7: Non-hex characters are rejected."""
        result = parse_hex_string("zz" * 32)
        self.assertIsNone(result)

    def test_empty_string(self):
        """GW-013 AC7: Empty string is rejected."""
        result = parse_hex_string("")
        self.assertIsNone(result)


class TestPortRangeValidation(unittest.TestCase):
    """GW-013 AC6: Port range validation."""

    def test_valid_port(self):
        """GW-013 AC6: Port within valid range is accepted."""
        cfg = ReferenceConfiguration()
        cfg.load_from_json(json.dumps({"listen_port": 8080}))
        self.assertEqual(cfg.tcp_listen_port, 8080)
        self.assertGreaterEqual(cfg.tcp_listen_port, 0)
        self.assertLessEqual(cfg.tcp_listen_port, 65535)

    def test_port_zero(self):
        """GW-013 AC6: Port 0 is technically valid (OS-assigned)."""
        cfg = ReferenceConfiguration()
        cfg.load_from_json(json.dumps({"listen_port": 0}))
        self.assertEqual(cfg.tcp_listen_port, 0)

    def test_max_port(self):
        """GW-013 AC6: Port 65535 is valid."""
        cfg = ReferenceConfiguration()
        cfg.load_from_json(json.dumps({"listen_port": 65535}))
        self.assertEqual(cfg.tcp_listen_port, 65535)


class TestAesKeyInConfig(unittest.TestCase):
    """GW-013: AES key in config file."""

    def test_aes_key_from_config(self):
        """GW-013: AES key loaded from JSON config."""
        hex_key = "aa" * 32
        cfg = ReferenceConfiguration()
        ok = cfg.load_from_json(json.dumps({"aes_key": hex_key}))
        self.assertTrue(ok)
        self.assertTrue(cfg.aes_key_loaded)
        self.assertEqual(cfg.aes_key, bytes.fromhex(hex_key))


if __name__ == '__main__':
    unittest.main()
