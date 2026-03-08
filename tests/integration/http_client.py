#!/usr/bin/env python3
"""
HTTP Client Integration Tests for the Dynamic REST API Gateway Server.

Sends HTTP requests to the gateway to exercise all registered endpoints and
verify correct behaviour for authentication, validation, routing, and error
handling.

Uses only the Python standard library (http.client, json, socket).
"""

import http.client
import json
import sys
import time
import argparse
import socket

# ---------------------------------------------------------------------------
# Test framework helpers
# ---------------------------------------------------------------------------

class TestResult:
    """Accumulates pass/fail counts and prints a summary."""

    def __init__(self):
        self.passed = 0
        self.failed = 0
        self.errors = []

    def record(self, name, passed, detail=""):
        if passed:
            self.passed += 1
            print(f"  [PASS] {name}")
        else:
            self.failed += 1
            self.errors.append((name, detail))
            print(f"  [FAIL] {name}  -- {detail}")

    def summary(self):
        total = self.passed + self.failed
        print("\n" + "=" * 70)
        print(f"TEST RESULTS:  {self.passed}/{total} passed, {self.failed} failed")
        if self.errors:
            print("\nFailed tests:")
            for name, detail in self.errors:
                print(f"  - {name}: {detail}")
        print("=" * 70)
        return self.failed == 0


results = TestResult()

# ---------------------------------------------------------------------------
# HTTP helpers
# ---------------------------------------------------------------------------

def make_request(host, port, method, path, body=None, headers=None, timeout=10):
    """
    Send an HTTP request and return (status, response_headers, body_str).
    Returns (None, None, error_str) on connection failure.
    """
    try:
        conn = http.client.HTTPConnection(host, port, timeout=timeout)
        hdrs = {"Content-Type": "application/json", "Accept": "application/json"}
        if headers:
            hdrs.update(headers)
        body_bytes = None
        if body is not None:
            body_bytes = json.dumps(body).encode("utf-8") if isinstance(body, (dict, list)) else body.encode("utf-8")
        conn.request(method, path, body=body_bytes, headers=hdrs)
        resp = conn.getresponse()
        resp_body = resp.read().decode("utf-8")
        resp_headers = dict(resp.getheaders())
        status = resp.status
        conn.close()
        return status, resp_headers, resp_body
    except Exception as exc:
        return None, None, str(exc)


def make_raw_request(host, port, raw_data, timeout=5):
    """Send raw bytes over TCP and return the raw response string."""
    try:
        sock = socket.create_connection((host, port), timeout=timeout)
        if isinstance(raw_data, str):
            raw_data = raw_data.encode("utf-8")
        sock.sendall(raw_data)
        response = b""
        while True:
            try:
                chunk = sock.recv(4096)
                if not chunk:
                    break
                response += chunk
            except socket.timeout:
                break
        sock.close()
        return response.decode("utf-8", errors="replace")
    except Exception as exc:
        return f"ERROR: {exc}"


def parse_json_body(body_str):
    """Parse JSON body, returning dict or None."""
    try:
        return json.loads(body_str)
    except (json.JSONDecodeError, TypeError):
        return None

# ---------------------------------------------------------------------------
# Test cases
# ---------------------------------------------------------------------------

def test_health_endpoint(host, port):
    """Test GET /api/v1/health (unauthenticated)."""
    print("\n--- Health Endpoint ---")
    status, hdrs, body = make_request(host, port, "GET", "/api/v1/health")
    data = parse_json_body(body)

    results.record(
        "GET /api/v1/health returns 200",
        status == 200,
        f"status={status}, body={body[:200] if body else 'None'}",
    )
    results.record(
        "Health response contains status field",
        data is not None and "status" in str(data),
        f"body={data}",
    )


def test_login_success(host, port):
    """Test POST /api/v1/login with valid credentials."""
    print("\n--- Login (success) ---")
    status, hdrs, body = make_request(
        host, port, "POST", "/api/v1/login",
        body={"username": "admin", "password": "admin123"},
    )
    data = parse_json_body(body)

    results.record(
        "POST /api/v1/login returns 200",
        status == 200,
        f"status={status}, body={body[:300] if body else 'None'}",
    )
    has_tokens = (
        data is not None
        and "access_token" in data
        and "refresh_token" in data
    )
    results.record(
        "Login response contains access_token and refresh_token",
        has_tokens,
        f"keys={list(data.keys()) if data else 'None'}",
    )
    return data if has_tokens else None


def test_login_failure(host, port):
    """Test POST /api/v1/login with invalid credentials."""
    print("\n--- Login (failure) ---")
    status, hdrs, body = make_request(
        host, port, "POST", "/api/v1/login",
        body={"username": "admin", "password": "wrongpassword"},
    )
    results.record(
        "POST /api/v1/login with bad creds returns 401",
        status == 401,
        f"status={status}, body={body[:200] if body else 'None'}",
    )


def test_authenticated_get_user(host, port, access_token):
    """Test GET /api/v1/users/{id} with valid token."""
    print("\n--- Get User (authenticated) ---")
    status, hdrs, body = make_request(
        host, port, "GET", "/api/v1/users/1",
        headers={"Authorization": f"Bearer {access_token}"},
    )
    data = parse_json_body(body)

    results.record(
        "GET /api/v1/users/1 returns 200",
        status == 200,
        f"status={status}, body={body[:200] if body else 'None'}",
    )
    results.record(
        "User response contains user data",
        data is not None and ("name" in str(data) or "id" in str(data)),
        f"body={data}",
    )


def test_authenticated_get_user_not_found(host, port, access_token):
    """Test GET /api/v1/users/{id} with non-existent user."""
    print("\n--- Get User (not found) ---")
    status, hdrs, body = make_request(
        host, port, "GET", "/api/v1/users/9999",
        headers={"Authorization": f"Bearer {access_token}"},
    )
    results.record(
        "GET /api/v1/users/9999 returns 404",
        status == 404,
        f"status={status}, body={body[:200] if body else 'None'}",
    )


def test_create_user(host, port, access_token):
    """Test POST /api/v1/users with valid data."""
    print("\n--- Create User ---")
    status, hdrs, body = make_request(
        host, port, "POST", "/api/v1/users",
        body={"name": "New User", "email": "newuser@test.com", "age": 28},
        headers={"Authorization": f"Bearer {access_token}"},
    )
    data = parse_json_body(body)

    results.record(
        "POST /api/v1/users returns 200 or 201",
        status in (200, 201),
        f"status={status}, body={body[:200] if body else 'None'}",
    )
    results.record(
        "Created user has name field",
        data is not None and "name" in str(data),
        f"body={data}",
    )


def test_update_user(host, port, access_token):
    """Test PUT /api/v1/users/{id}."""
    print("\n--- Update User ---")
    status, hdrs, body = make_request(
        host, port, "PUT", "/api/v1/users/1",
        body={"name": "Alice Updated"},
        headers={"Authorization": f"Bearer {access_token}"},
    )
    results.record(
        "PUT /api/v1/users/1 returns 200",
        status == 200,
        f"status={status}, body={body[:200] if body else 'None'}",
    )


def test_delete_user(host, port, access_token):
    """Test DELETE /api/v1/users/{id}."""
    print("\n--- Delete User ---")
    status, hdrs, body = make_request(
        host, port, "DELETE", "/api/v1/users/3",
        headers={"Authorization": f"Bearer {access_token}"},
    )
    results.record(
        "DELETE /api/v1/users/3 returns 200",
        status == 200,
        f"status={status}, body={body[:200] if body else 'None'}",
    )


def test_list_orders(host, port, access_token):
    """Test GET /api/v1/orders with query params."""
    print("\n--- List Orders ---")
    status, hdrs, body = make_request(
        host, port, "GET", "/api/v1/orders?page=1&limit=10",
        headers={"Authorization": f"Bearer {access_token}"},
    )
    data = parse_json_body(body)

    results.record(
        "GET /api/v1/orders returns 200",
        status == 200,
        f"status={status}, body={body[:200] if body else 'None'}",
    )
    results.record(
        "Orders response contains list data",
        data is not None and ("orders" in str(data) or "total" in str(data)),
        f"body={data}",
    )


def test_get_order(host, port, access_token):
    """Test GET /api/v1/orders/{orderId}."""
    print("\n--- Get Order ---")
    status, hdrs, body = make_request(
        host, port, "GET", "/api/v1/orders/1",
        headers={"Authorization": f"Bearer {access_token}"},
    )
    data = parse_json_body(body)

    results.record(
        "GET /api/v1/orders/1 returns 200",
        status == 200,
        f"status={status}, body={body[:200] if body else 'None'}",
    )
    results.record(
        "Order response contains order data",
        data is not None and ("product" in str(data) or "id" in str(data)),
        f"body={data}",
    )


def test_unauthenticated_access(host, port):
    """Test that authenticated endpoints reject requests without a token."""
    print("\n--- Unauthenticated Access ---")
    status, hdrs, body = make_request(
        host, port, "GET", "/api/v1/users/1",
    )
    results.record(
        "GET /api/v1/users/1 without token returns 401",
        status == 401,
        f"status={status}, body={body[:200] if body else 'None'}",
    )


def test_invalid_token(host, port):
    """Test that a bad token is rejected."""
    print("\n--- Invalid Token ---")
    status, hdrs, body = make_request(
        host, port, "GET", "/api/v1/users/1",
        headers={"Authorization": "Bearer invalid_token_value_here"},
    )
    results.record(
        "GET /api/v1/users/1 with bad token returns 401",
        status == 401,
        f"status={status}, body={body[:200] if body else 'None'}",
    )


def test_token_refresh(host, port, refresh_token):
    """Test POST /auth/refresh."""
    print("\n--- Token Refresh ---")
    status, hdrs, body = make_request(
        host, port, "POST", "/auth/refresh",
        body={"refresh_token": refresh_token},
    )
    data = parse_json_body(body)

    results.record(
        "POST /auth/refresh returns 200",
        status == 200,
        f"status={status}, body={body[:300] if body else 'None'}",
    )
    has_tokens = (
        data is not None
        and "access_token" in data
        and "refresh_token" in data
    )
    results.record(
        "Refresh response contains new token pair",
        has_tokens,
        f"keys={list(data.keys()) if data else 'None'}",
    )
    return data


def test_invalid_refresh_token(host, port):
    """Test POST /auth/refresh with an invalid token."""
    print("\n--- Invalid Refresh Token ---")
    status, hdrs, body = make_request(
        host, port, "POST", "/auth/refresh",
        body={"refresh_token": "totally_bogus_refresh_token"},
    )
    results.record(
        "POST /auth/refresh with bad token returns 401",
        status == 401,
        f"status={status}, body={body[:200] if body else 'None'}",
    )


def test_unknown_route(host, port):
    """Test that an unregistered route returns 404."""
    print("\n--- Unknown Route ---")
    status, hdrs, body = make_request(host, port, "GET", "/api/v1/nonexistent")
    results.record(
        "GET /api/v1/nonexistent returns 404",
        status == 404,
        f"status={status}, body={body[:200] if body else 'None'}",
    )


def test_method_not_registered(host, port):
    """Test that a valid path with wrong method returns 404 (no route)."""
    print("\n--- Wrong Method ---")
    status, hdrs, body = make_request(host, port, "PATCH", "/api/v1/health")
    results.record(
        "PATCH /api/v1/health returns 404 or 405",
        status in (404, 405),
        f"status={status}, body={body[:200] if body else 'None'}",
    )


def test_validation_missing_required_field(host, port, access_token):
    """Test POST /api/v1/users with a missing required field."""
    print("\n--- Validation: Missing Required Field ---")
    # Missing 'email' which is required
    status, hdrs, body = make_request(
        host, port, "POST", "/api/v1/users",
        body={"name": "No Email User"},
        headers={"Authorization": f"Bearer {access_token}"},
    )
    results.record(
        "POST /api/v1/users missing email returns 400",
        status == 400,
        f"status={status}, body={body[:200] if body else 'None'}",
    )


def test_validation_invalid_type(host, port, access_token):
    """Test GET /api/v1/users/{id} with a non-integer path parameter."""
    print("\n--- Validation: Invalid Path Param Type ---")
    status, hdrs, body = make_request(
        host, port, "GET", "/api/v1/users/abc",
        headers={"Authorization": f"Bearer {access_token}"},
    )
    results.record(
        "GET /api/v1/users/abc returns 400",
        status == 400,
        f"status={status}, body={body[:200] if body else 'None'}",
    )


def test_malformed_http(host, port):
    """Test that malformed HTTP produces a 400 response."""
    print("\n--- Malformed HTTP ---")
    raw = "GIBBERISH_NOT_HTTP\r\n\r\n"
    resp = make_raw_request(host, port, raw)
    results.record(
        "Malformed HTTP returns 400 status line",
        "400" in resp,
        f"response starts with: {resp[:200]}",
    )


def test_empty_body_on_post(host, port):
    """Test POST /api/v1/login with empty body."""
    print("\n--- Empty Body on POST ---")
    status, hdrs, body = make_request(
        host, port, "POST", "/api/v1/login",
        body={},
    )
    # The gateway should either reject (400) or the backend returns 401
    results.record(
        "POST /api/v1/login with empty body returns 400 or 401",
        status in (400, 401),
        f"status={status}, body={body[:200] if body else 'None'}",
    )


# ---------------------------------------------------------------------------
# Main entry point
# ---------------------------------------------------------------------------

def run_all_tests(host, port):
    """Run the full integration test suite."""
    print("=" * 70)
    print(f"HTTP Client Integration Tests  --  Gateway at {host}:{port}")
    print("=" * 70)

    # 1. Unauthenticated endpoints
    test_health_endpoint(host, port)

    # 2. Login flow
    test_login_failure(host, port)
    login_data = test_login_success(host, port)

    access_token = login_data.get("access_token") if login_data else None
    refresh_token = login_data.get("refresh_token") if login_data else None

    if not access_token:
        print("\n[SKIP] Skipping authenticated tests - no access token obtained")
        results.record("Login produced usable token", False, "no token")
        results.summary()
        return

    # 3. Authenticated CRUD endpoints
    test_authenticated_get_user(host, port, access_token)
    test_authenticated_get_user_not_found(host, port, access_token)
    test_create_user(host, port, access_token)
    test_update_user(host, port, access_token)
    test_delete_user(host, port, access_token)
    test_list_orders(host, port, access_token)
    test_get_order(host, port, access_token)

    # 4. Auth edge cases
    test_unauthenticated_access(host, port)
    test_invalid_token(host, port)

    # 5. Token refresh
    if refresh_token:
        refreshed = test_token_refresh(host, port, refresh_token)
        test_invalid_refresh_token(host, port)
    else:
        print("\n[SKIP] Skipping refresh tests - no refresh token")

    # 6. Validation
    test_validation_missing_required_field(host, port, access_token)
    test_validation_invalid_type(host, port, access_token)

    # 7. Error handling
    test_unknown_route(host, port)
    test_method_not_registered(host, port)
    test_malformed_http(host, port)
    test_empty_body_on_post(host, port)

    # Summary
    all_passed = results.summary()
    sys.exit(0 if all_passed else 1)


def main():
    parser = argparse.ArgumentParser(description="HTTP Client Integration Tests for Gateway")
    parser.add_argument("--host", default="127.0.0.1",
                        help="Gateway host (default: 127.0.0.1)")
    parser.add_argument("--port", "-p", type=int, default=8080,
                        help="Gateway port (default: 8080)")
    args = parser.parse_args()

    run_all_tests(args.host, args.port)


if __name__ == "__main__":
    main()
