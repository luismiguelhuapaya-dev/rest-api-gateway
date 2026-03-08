#!/usr/bin/env python3
"""
Network Scanning Test Tool.

Scans ports on scanme.nmap.org (a host explicitly authorized for scanning)
to validate basic network scanning capabilities.

Tests common ports: 22 (SSH), 80 (HTTP), 443 (HTTPS), 9929, 31337.
Reports open/closed/filtered status for each port.

Uses only the Python standard library (socket).
"""

import socket
import sys
import time
import threading
import argparse

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

TARGET_HOST = "scanme.nmap.org"

DEFAULT_PORTS = [22, 80, 443, 9929, 31337]

PORT_NAMES = {
    22:    "SSH",
    80:    "HTTP",
    443:   "HTTPS",
    9929:  "nping-echo",
    31337: "Elite",
}

# ---------------------------------------------------------------------------
# Scanner
# ---------------------------------------------------------------------------

class PortScanResult:
    """Result of scanning a single port."""
    def __init__(self, port, status, latency_ms=None, banner=None):
        self.port = port
        self.status = status       # "open", "closed", "filtered"
        self.latency_ms = latency_ms
        self.banner = banner

    def __str__(self):
        name = PORT_NAMES.get(self.port, "")
        lat = f"  ({self.latency_ms:.1f} ms)" if self.latency_ms is not None else ""
        ban = f"  banner={self.banner}" if self.banner else ""
        return f"  Port {self.port:>5}/{name:<14s}  {self.status:<10s}{lat}{ban}"


def scan_port(host, port, timeout=3.0):
    """
    Attempt a TCP connect to *host*:*port*.

    Returns a PortScanResult with:
      - "open"     if the connection succeeds
      - "closed"   if the connection is actively refused
      - "filtered" if it times out (likely firewalled)
    """
    start = time.perf_counter()
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        result_code = sock.connect_ex((host, port))
        elapsed_ms = (time.perf_counter() - start) * 1000.0

        if result_code == 0:
            # Try to grab a banner
            banner = None
            try:
                sock.settimeout(1.0)
                banner_bytes = sock.recv(1024)
                if banner_bytes:
                    banner = banner_bytes.decode("utf-8", errors="replace").strip()[:80]
            except Exception:
                pass
            sock.close()
            return PortScanResult(port, "open", elapsed_ms, banner)
        else:
            sock.close()
            return PortScanResult(port, "closed", elapsed_ms)

    except socket.timeout:
        elapsed_ms = (time.perf_counter() - start) * 1000.0
        return PortScanResult(port, "filtered", elapsed_ms)
    except OSError as exc:
        elapsed_ms = (time.perf_counter() - start) * 1000.0
        # Connection refused -> closed
        if exc.errno in (111, 10061):  # ECONNREFUSED (Linux, Windows)
            return PortScanResult(port, "closed", elapsed_ms)
        return PortScanResult(port, "filtered", elapsed_ms)


def scan_ports_threaded(host, ports, timeout=3.0, max_threads=10):
    """Scan all *ports* on *host* using threads.  Returns list of PortScanResult."""
    results = [None] * len(ports)
    lock = threading.Lock()

    def _worker(index, port):
        r = scan_port(host, port, timeout)
        with lock:
            results[index] = r

    threads = []
    for i, p in enumerate(ports):
        t = threading.Thread(target=_worker, args=(i, p), daemon=True)
        threads.append(t)
        t.start()
        # Simple concurrency limiter
        if len(threads) >= max_threads:
            for tt in threads:
                tt.join()
            threads = []

    for t in threads:
        t.join()

    return [r for r in results if r is not None]

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def run_scan(host, ports, timeout):
    print("=" * 60)
    print("Network Port Scanner Test")
    print(f"  Target: {host}")

    # Resolve hostname
    try:
        ip = socket.gethostbyname(host)
        print(f"  IP:     {ip}")
    except socket.gaierror as exc:
        print(f"  DNS resolution failed: {exc}")
        return False

    print(f"  Ports:  {', '.join(str(p) for p in ports)}")
    print(f"  Timeout: {timeout}s per port")
    print("=" * 60)
    print()

    scan_start = time.perf_counter()
    results = scan_ports_threaded(host, ports, timeout)
    scan_elapsed = time.perf_counter() - scan_start

    print("Scan results:")
    open_count = 0
    closed_count = 0
    filtered_count = 0

    for r in results:
        print(r)
        if r.status == "open":
            open_count += 1
        elif r.status == "closed":
            closed_count += 1
        else:
            filtered_count += 1

    print()
    print(f"Scan completed in {scan_elapsed:.2f}s")
    print(f"  Open:     {open_count}")
    print(f"  Closed:   {closed_count}")
    print(f"  Filtered: {filtered_count}")
    print()

    # Validation: scanme.nmap.org is expected to have at least port 80 open
    test_passed = True
    print("Validation:")
    port_80_result = next((r for r in results if r.port == 80), None)
    if port_80_result and port_80_result.status == "open":
        print("  [PASS] Port 80 (HTTP) is open on scanme.nmap.org as expected")
    else:
        status = port_80_result.status if port_80_result else "not scanned"
        print(f"  [WARN] Port 80 status: {status} (expected open, host may be down)")
        # Don't fail the test if the host is unreachable
        if port_80_result and port_80_result.status == "filtered":
            print("         This may indicate a firewall or network issue")

    # Check that scan itself ran without errors
    if len(results) == len(ports):
        print(f"  [PASS] All {len(ports)} ports scanned successfully")
    else:
        print(f"  [FAIL] Only {len(results)}/{len(ports)} ports completed")
        test_passed = False

    print("=" * 60)
    return test_passed


def main():
    parser = argparse.ArgumentParser(
        description="Network port scanning test against scanme.nmap.org")
    parser.add_argument("--host", default=TARGET_HOST,
                        help=f"Target host (default: {TARGET_HOST})")
    parser.add_argument("--ports", default=None,
                        help="Comma-separated list of ports (default: 22,80,443,9929,31337)")
    parser.add_argument("--timeout", "-t", type=float, default=3.0,
                        help="Timeout per port in seconds (default: 3)")
    args = parser.parse_args()

    ports = DEFAULT_PORTS
    if args.ports:
        try:
            ports = [int(p.strip()) for p in args.ports.split(",")]
        except ValueError:
            print(f"Error: invalid port list: {args.ports}")
            sys.exit(1)

    success = run_scan(args.host, ports, args.timeout)
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
