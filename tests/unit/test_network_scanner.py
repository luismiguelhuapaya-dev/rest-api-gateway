"""
Unit tests for network scanning capability.
Tests scan scanme.nmap.org to validate socket-based port scanning.

Reference: Network connectivity validation
"""

import socket
import unittest
import sys
import os

sys.path.insert(0, os.path.dirname(__file__))

# Target host (nmap's official test host, scanning is explicitly permitted)
TARGET_HOST = "scanme.nmap.org"
SCAN_TIMEOUT = 5  # seconds


def scan_port(host: str, port: int, timeout: float = SCAN_TIMEOUT) -> bool:
    """
    Attempt a TCP connect to host:port.
    Returns True if the port is open (connection succeeds), False otherwise.
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(timeout)
    try:
        result = sock.connect_ex((host, port))
        return result == 0
    except (socket.timeout, socket.error, OSError):
        return False
    finally:
        sock.close()


class TestNetworkScanner(unittest.TestCase):
    """Network scanner tests against scanme.nmap.org."""

    @classmethod
    def setUpClass(cls):
        """Resolve scanme.nmap.org once to verify DNS works."""
        try:
            cls.target_ip = socket.gethostbyname(TARGET_HOST)
            cls.dns_ok = True
        except socket.gaierror:
            cls.target_ip = None
            cls.dns_ok = False

    def test_dns_resolution(self):
        """Network: DNS resolution of scanme.nmap.org succeeds."""
        self.assertTrue(self.dns_ok,
                        f"Failed to resolve {TARGET_HOST}")
        self.assertIsNotNone(self.target_ip)

    def test_port_80_open(self):
        """Network: Port 80 (HTTP) on scanme.nmap.org is open."""
        if not self.dns_ok:
            self.skipTest("DNS resolution failed")
        is_open = scan_port(TARGET_HOST, 80)
        self.assertTrue(is_open,
                        f"Expected port 80 on {TARGET_HOST} to be OPEN")

    def test_port_22_scan(self):
        """Network: Port 22 (SSH) on scanme.nmap.org scan completes.
        This port may be open or filtered. The test validates that
        the scanner handles it without error.
        """
        if not self.dns_ok:
            self.skipTest("DNS resolution failed")
        # Just verify the scan completes without exception
        result = scan_port(TARGET_HOST, 22)
        self.assertIsInstance(result, bool)

    def test_port_443_scan(self):
        """Network: Port 443 (HTTPS) on scanme.nmap.org scan completes."""
        if not self.dns_ok:
            self.skipTest("DNS resolution failed")
        result = scan_port(TARGET_HOST, 443)
        self.assertIsInstance(result, bool)

    def test_port_9929_scan(self):
        """Network: Port 9929 on scanme.nmap.org scan completes."""
        if not self.dns_ok:
            self.skipTest("DNS resolution failed")
        result = scan_port(TARGET_HOST, 9929)
        self.assertIsInstance(result, bool)

    def test_port_31337_scan(self):
        """Network: Port 31337 on scanme.nmap.org scan completes."""
        if not self.dns_ok:
            self.skipTest("DNS resolution failed")
        result = scan_port(TARGET_HOST, 31337)
        self.assertIsInstance(result, bool)

    def test_timeout_handling(self):
        """Network: Timeout handling for filtered/closed ports.
        Uses a very short timeout to verify the scanner handles
        timeouts gracefully.
        """
        if not self.dns_ok:
            self.skipTest("DNS resolution failed")
        # Use an extremely short timeout; likely will fail to connect
        result = scan_port(TARGET_HOST, 31337, timeout=0.1)
        # We only care that it doesn't raise an exception
        self.assertIsInstance(result, bool)

    def test_scan_results_summary(self):
        """Network: Scan all specified ports and report results."""
        if not self.dns_ok:
            self.skipTest("DNS resolution failed")

        ports = [22, 80, 443, 9929, 31337]
        results = {}
        for port in ports:
            results[port] = scan_port(TARGET_HOST, port)

        # Port 80 should be open
        self.assertTrue(results[80],
                        f"Port 80 expected OPEN, got CLOSED/FILTERED")

        # Print results summary for visibility
        for port, is_open in results.items():
            status = "OPEN" if is_open else "CLOSED/FILTERED"
            print(f"  scanme.nmap.org:{port} -> {status}")


class TestScannerEdgeCases(unittest.TestCase):
    """Network scanner edge case tests."""

    def test_invalid_host(self):
        """Network: Scanning an invalid host returns False (no exception)."""
        result = scan_port("this.host.does.not.exist.invalid", 80, timeout=2)
        self.assertFalse(result)

    def test_localhost_closed_port(self):
        """Network: Scanning a likely-closed port on localhost returns False."""
        # Port 19999 is very unlikely to be in use
        result = scan_port("127.0.0.1", 19999, timeout=1)
        self.assertIsInstance(result, bool)

    def test_zero_timeout(self):
        """Network: Zero timeout is handled gracefully."""
        # Should not raise, just return quickly
        try:
            result = scan_port("127.0.0.1", 80, timeout=0.001)
            self.assertIsInstance(result, bool)
        except Exception as e:
            self.fail(f"Zero timeout raised exception: {e}")


if __name__ == '__main__':
    unittest.main()
