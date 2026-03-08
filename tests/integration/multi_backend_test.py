#!/usr/bin/env python3
"""
Multi-Backend Integration Test for the Dynamic REST API Gateway Server.

Tests hot registration/deregistration by spinning up multiple backend
simulators that connect to the gateway via Unix domain sockets, verifying:
  - Routing to different backends
  - Disconnection handling
  - Re-registration after disconnect

Uses only the Python standard library.
"""

import socket
import struct
import json
import sys
import time
import threading
import argparse
import http.client
import logging

# ---------------------------------------------------------------------------
# Frame protocol constants (must match FrameProtocol.h exactly)
# ---------------------------------------------------------------------------

FRAME_HEADER_SIZE = 12

FRAME_TYPE_REGISTRATION   = 0
FRAME_TYPE_UNREGISTRATION = 1
FRAME_TYPE_REQUEST        = 2
FRAME_TYPE_RESPONSE       = 3
FRAME_TYPE_LOGIN_RESPONSE = 4
FRAME_TYPE_TOKEN_RESPONSE = 5
FRAME_TYPE_HEARTBEAT      = 6
FRAME_TYPE_ERROR          = 7

logging.basicConfig(
    level=logging.INFO,
    format="[%(asctime)s] %(levelname)-8s %(name)s: %(message)s",
    datefmt="%H:%M:%S",
)

# ---------------------------------------------------------------------------
# Frame helpers (same as backend_simulator.py)
# ---------------------------------------------------------------------------

def read_exact(sock, n):
    buf = b""
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("Connection closed")
        buf += chunk
    return buf


def read_frame(sock):
    header = read_exact(sock, FRAME_HEADER_SIZE)
    frame_type, request_id, payload_len = struct.unpack("!III", header)
    payload = read_exact(sock, payload_len) if payload_len > 0 else b""
    return frame_type, request_id, payload.decode("utf-8")


def send_frame(sock, frame_type, request_id, payload):
    if isinstance(payload, str):
        payload = payload.encode("utf-8")
    header = struct.pack("!III", frame_type, request_id, len(payload))
    sock.sendall(header + payload)


def http_get(host, port, path, headers=None, timeout=5):
    try:
        conn = http.client.HTTPConnection(host, port, timeout=timeout)
        hdrs = {"Accept": "application/json"}
        if headers:
            hdrs.update(headers)
        conn.request("GET", path, headers=hdrs)
        resp = conn.getresponse()
        body = resp.read().decode("utf-8")
        status = resp.status
        conn.close()
        return status, body
    except Exception as exc:
        return None, str(exc)

# ---------------------------------------------------------------------------
# Mini backend
# ---------------------------------------------------------------------------

class MiniBackend:
    """
    A lightweight backend simulator that registers a single endpoint and
    responds with a fixed JSON payload.
    """

    def __init__(self, socket_path, backend_id, endpoint_path, response_body):
        self.log = logging.getLogger(f"Backend[{backend_id}]")
        self.socket_path = socket_path
        self.backend_id = backend_id
        self.endpoint_path = endpoint_path
        self.response_body = response_body
        self.sock = None
        self.running = False
        self.request_count = 0
        self._thread = None

    def connect_and_register(self):
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.connect(self.socket_path)
        self.log.info("Connected to gateway")

        registration = json.dumps({
            "backend_id": self.backend_id,
            "endpoints": [
                {
                    "path": self.endpoint_path,
                    "method": "GET",
                    "description": f"Test endpoint for {self.backend_id}",
                    "requires_auth": False,
                    "parameters": [],
                }
            ],
        })
        send_frame(self.sock, FRAME_TYPE_REGISTRATION, 0, registration)
        self.log.info("Sent registration")

        # Wait for ack
        ft, rid, payload = read_frame(self.sock)
        if ft == FRAME_TYPE_RESPONSE:
            self.log.info("Registration acknowledged: %s", payload[:120])
        else:
            self.log.warning("Unexpected ack frame type %d", ft)

    def _loop(self):
        self.running = True
        while self.running:
            try:
                ft, rid, payload = read_frame(self.sock)
                if ft == FRAME_TYPE_REQUEST:
                    self.request_count += 1
                    resp = json.dumps({
                        "status_code": 200,
                        "body": self.response_body,
                    })
                    send_frame(self.sock, FRAME_TYPE_RESPONSE, rid, resp)
                    self.log.info("Handled request #%d", self.request_count)
                elif ft == FRAME_TYPE_HEARTBEAT:
                    send_frame(self.sock, FRAME_TYPE_HEARTBEAT, rid,
                               json.dumps({"status": "alive"}))
                else:
                    self.log.debug("Frame type %d ignored", ft)
            except ConnectionError:
                self.log.info("Connection closed")
                self.running = False
            except Exception as exc:
                self.log.error("Error: %s", exc)
                self.running = False

    def start(self):
        self.connect_and_register()
        self._thread = threading.Thread(target=self._loop, daemon=True)
        self._thread.start()

    def stop(self):
        self.running = False
        if self.sock:
            try:
                send_frame(self.sock, FRAME_TYPE_UNREGISTRATION, 0,
                           json.dumps({"backend_id": self.backend_id}))
            except Exception:
                pass
            self.sock.close()
            self.sock = None
        if self._thread:
            self._thread.join(timeout=3)
        self.log.info("Stopped")

    def disconnect_abruptly(self):
        """Simulate a crash by closing the socket without unregistering."""
        self.running = False
        if self.sock:
            self.sock.close()
            self.sock = None
        if self._thread:
            self._thread.join(timeout=3)
        self.log.info("Disconnected abruptly (simulated crash)")

# ---------------------------------------------------------------------------
# Test runner
# ---------------------------------------------------------------------------

class TestResult:
    def __init__(self):
        self.passed = 0
        self.failed = 0
        self.details = []

    def record(self, name, ok, detail=""):
        if ok:
            self.passed += 1
            print(f"  [PASS] {name}")
        else:
            self.failed += 1
            self.details.append((name, detail))
            print(f"  [FAIL] {name}  -- {detail}")

    def summary(self):
        total = self.passed + self.failed
        print()
        print("=" * 60)
        print(f"MULTI-BACKEND TEST:  {self.passed}/{total} passed, {self.failed} failed")
        if self.details:
            print("  Failed:")
            for n, d in self.details:
                print(f"    - {n}: {d}")
        print("=" * 60)
        return self.failed == 0


def run_tests(gateway_host, gateway_port, socket_path):
    print("=" * 60)
    print("Multi-Backend Integration Test")
    print(f"  Gateway HTTP: {gateway_host}:{gateway_port}")
    print(f"  Unix socket:  {socket_path}")
    print("=" * 60)

    results = TestResult()

    # ------------------------------------------------------------------
    # Test 1: Two backends serving different endpoints
    # ------------------------------------------------------------------
    print("\n--- Test 1: Two backends with different endpoints ---")

    backend_a = MiniBackend(
        socket_path, "backend_a", "/api/v1/service-a/ping",
        {"service": "A", "message": "pong from A"},
    )
    backend_b = MiniBackend(
        socket_path, "backend_b", "/api/v1/service-b/ping",
        {"service": "B", "message": "pong from B"},
    )

    try:
        backend_a.start()
        backend_b.start()
        time.sleep(0.5)  # Let registrations settle

        status_a, body_a = http_get(gateway_host, gateway_port, "/api/v1/service-a/ping")
        results.record("Route to backend A returns 200", status_a == 200,
                        f"status={status_a} body={body_a[:200] if body_a else ''}")

        status_b, body_b = http_get(gateway_host, gateway_port, "/api/v1/service-b/ping")
        results.record("Route to backend B returns 200", status_b == 200,
                        f"status={status_b} body={body_b[:200] if body_b else ''}")

        # Verify payloads
        if body_a:
            try:
                data_a = json.loads(body_a)
                results.record("Backend A payload matches",
                               "A" in str(data_a),
                               f"data={data_a}")
            except json.JSONDecodeError:
                results.record("Backend A payload is valid JSON", False, body_a[:200])

        if body_b:
            try:
                data_b = json.loads(body_b)
                results.record("Backend B payload matches",
                               "B" in str(data_b),
                               f"data={data_b}")
            except json.JSONDecodeError:
                results.record("Backend B payload is valid JSON", False, body_b[:200])

    finally:
        backend_a.stop()
        backend_b.stop()
        time.sleep(0.3)

    # ------------------------------------------------------------------
    # Test 2: Graceful unregistration removes routes
    # ------------------------------------------------------------------
    print("\n--- Test 2: Graceful unregistration ---")

    backend_c = MiniBackend(
        socket_path, "backend_c", "/api/v1/service-c/ping",
        {"service": "C"},
    )
    try:
        backend_c.start()
        time.sleep(0.3)

        status, _ = http_get(gateway_host, gateway_port, "/api/v1/service-c/ping")
        results.record("Service C reachable before unregister", status == 200,
                        f"status={status}")

        backend_c.stop()
        time.sleep(0.5)

        status, body = http_get(gateway_host, gateway_port, "/api/v1/service-c/ping")
        results.record("Service C unreachable after unregister",
                        status in (404, 500, None),
                        f"status={status}")
    except Exception as exc:
        results.record("Unregistration test", False, str(exc))

    # ------------------------------------------------------------------
    # Test 3: Abrupt disconnection
    # ------------------------------------------------------------------
    print("\n--- Test 3: Abrupt backend disconnection ---")

    backend_d = MiniBackend(
        socket_path, "backend_d", "/api/v1/service-d/ping",
        {"service": "D"},
    )
    try:
        backend_d.start()
        time.sleep(0.3)

        status, _ = http_get(gateway_host, gateway_port, "/api/v1/service-d/ping")
        results.record("Service D reachable before crash", status == 200,
                        f"status={status}")

        backend_d.disconnect_abruptly()
        time.sleep(0.5)

        status, body = http_get(gateway_host, gateway_port, "/api/v1/service-d/ping")
        results.record("Service D unreachable after crash",
                        status in (404, 500, 502, None),
                        f"status={status}")
    except Exception as exc:
        results.record("Abrupt disconnect test", False, str(exc))

    # ------------------------------------------------------------------
    # Test 4: Re-registration after disconnect
    # ------------------------------------------------------------------
    print("\n--- Test 4: Re-registration ---")

    backend_e = MiniBackend(
        socket_path, "backend_e", "/api/v1/service-e/ping",
        {"service": "E", "version": 1},
    )
    try:
        backend_e.start()
        time.sleep(0.3)

        status, _ = http_get(gateway_host, gateway_port, "/api/v1/service-e/ping")
        results.record("Service E v1 reachable", status == 200, f"status={status}")

        backend_e.stop()
        time.sleep(0.5)

        # Re-register with a new payload
        backend_e2 = MiniBackend(
            socket_path, "backend_e", "/api/v1/service-e/ping",
            {"service": "E", "version": 2},
        )
        backend_e2.start()
        time.sleep(0.3)

        status, body = http_get(gateway_host, gateway_port, "/api/v1/service-e/ping")
        results.record("Service E v2 reachable after re-register", status == 200,
                        f"status={status}")

        if body:
            try:
                data = json.loads(body)
                results.record("Service E v2 payload is version 2",
                               "2" in str(data.get("version", data)),
                               f"data={data}")
            except json.JSONDecodeError:
                results.record("Service E v2 payload valid JSON", False, body[:200])

        backend_e2.stop()
    except Exception as exc:
        results.record("Re-registration test", False, str(exc))

    # Summary
    return results.summary()


def main():
    parser = argparse.ArgumentParser(
        description="Multi-Backend Integration Test for Gateway")
    parser.add_argument("--host", default="127.0.0.1",
                        help="Gateway HTTP host (default: 127.0.0.1)")
    parser.add_argument("--port", "-p", type=int, default=8080,
                        help="Gateway HTTP port (default: 8080)")
    parser.add_argument("--socket", "-s", default="/tmp/gateway.sock",
                        help="Gateway Unix domain socket path (default: /tmp/gateway.sock)")
    args = parser.parse_args()

    success = run_tests(args.host, args.port, args.socket)
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
