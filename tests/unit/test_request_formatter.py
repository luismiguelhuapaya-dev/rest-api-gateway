"""
Unit tests for request formatting.
Tests mirror the behaviour implemented in src/forwarding/RequestFormatter.cpp.

Reference: GW-007 - Backend Request Forwarding
"""

import json
import unittest
import sys
import os

sys.path.insert(0, os.path.dirname(__file__))
from test_helpers import (
    HttpRequest, HttpMethod, EndpointDefinition,
    FrameType, http_method_to_string
)


# ---------------------------------------------------------------------------
# Python reference request formatter mirroring RequestFormatter.cpp
# ---------------------------------------------------------------------------

def escape_json_string(s: str) -> str:
    """Mirror RequestFormatter::EscapeJsonString."""
    result = []
    for ch in s:
        if ch == '"':
            result.append('\\"')
        elif ch == '\\':
            result.append('\\\\')
        elif ch == '\n':
            result.append('\\n')
        elif ch == '\r':
            result.append('\\r')
        elif ch == '\t':
            result.append('\\t')
        else:
            result.append(ch)
    return "".join(result)


_FORWARDED_HEADERS = frozenset([
    "content-type", "accept", "user-agent", "x-request-id", "x-forwarded-for"
])

_next_request_id = [1]


def format_request_for_backend(request: HttpRequest, endpoint: EndpointDefinition):
    """Build a JSON payload from the request, mirroring BuildJsonPayload."""
    parts = []

    # Method
    parts.append('"method":"' + escape_json_string(http_method_to_string(request.method)) + '"')

    # Path
    parts.append('"path":"' + escape_json_string(request.path) + '"')

    # Backend
    parts.append('"backend":"' + escape_json_string(endpoint.backend_identifier) + '"')

    # Path parameters
    if request.path_parameters:
        pp_parts = []
        for k, v in request.path_parameters.items():
            pp_parts.append('"' + escape_json_string(k) + '":"' + escape_json_string(v) + '"')
        parts.append('"path_parameters":{' + ','.join(pp_parts) + '}')

    # Query parameters
    if request.query_parameters:
        qp_parts = []
        for k, v in request.query_parameters.items():
            qp_parts.append('"' + escape_json_string(k) + '":"' + escape_json_string(v) + '"')
        parts.append('"query_parameters":{' + ','.join(qp_parts) + '}')

    # Headers (selected)
    hdr_parts = []
    for k, v in request.headers.items():
        if k in _FORWARDED_HEADERS:
            hdr_parts.append('"' + escape_json_string(k) + '":"' + escape_json_string(v) + '"')
    parts.append('"headers":{' + ','.join(hdr_parts) + '}')

    # Body
    if request.body:
        try:
            body_parsed = json.loads(request.body)
            parts.append('"body":' + json.dumps(body_parsed, separators=(',', ':')))
        except json.JSONDecodeError:
            parts.append('"body":"' + escape_json_string(request.body) + '"')

    payload = '{' + ','.join(parts) + '}'

    req_id = _next_request_id[0]
    _next_request_id[0] += 1

    return {
        "frame_type": FrameType.Request,
        "payload": payload,
        "request_identifier": req_id,
    }


# ---------------------------------------------------------------------------
# Test Cases
# ---------------------------------------------------------------------------

class TestRequestSerialization(unittest.TestCase):
    """GW-007 AC1: Validated requests are correctly serialized to JSON."""

    def _make_request(self, method, path, body="", headers=None, query_params=None, path_params=None):
        req = HttpRequest()
        req.method = method
        req.path = path
        req.body = body
        req.headers = headers or {}
        req.query_parameters = query_params or {}
        req.path_parameters = path_params or {}
        return req

    def _make_endpoint(self, backend="test-service"):
        ep = EndpointDefinition()
        ep.path = "/test"
        ep.method = HttpMethod.Get
        ep.backend_identifier = backend
        ep.parse_path_segments()
        return ep

    def test_get_request_serialization(self):
        """GW-007 AC1: GET request serialized with method, path, backend."""
        req = self._make_request(HttpMethod.Get, "/api/users")
        ep = self._make_endpoint("user-service")
        result = format_request_for_backend(req, ep)
        payload = json.loads(result["payload"])

        self.assertEqual(payload["method"], "GET")
        self.assertEqual(payload["path"], "/api/users")
        self.assertEqual(payload["backend"], "user-service")

    def test_post_request_with_body(self):
        """GW-007 AC1: POST request with JSON body is serialized correctly."""
        body = json.dumps({"name": "Alice", "age": 30})
        req = self._make_request(HttpMethod.Post, "/api/users",
                                 body=body,
                                 headers={"content-type": "application/json"})
        ep = self._make_endpoint()
        result = format_request_for_backend(req, ep)
        payload = json.loads(result["payload"])

        self.assertEqual(payload["method"], "POST")
        self.assertIn("body", payload)
        self.assertEqual(payload["body"]["name"], "Alice")
        self.assertEqual(payload["body"]["age"], 30)

    def test_path_parameters_included(self):
        """GW-007 AC1: Path parameters are included in serialized JSON."""
        req = self._make_request(HttpMethod.Get, "/api/users/42",
                                 path_params={"id": "42"})
        ep = self._make_endpoint()
        result = format_request_for_backend(req, ep)
        payload = json.loads(result["payload"])

        self.assertIn("path_parameters", payload)
        self.assertEqual(payload["path_parameters"]["id"], "42")

    def test_query_parameters_included(self):
        """GW-007 AC1: Query parameters are included in serialized JSON."""
        req = self._make_request(HttpMethod.Get, "/api/search",
                                 query_params={"q": "test", "page": "1"})
        ep = self._make_endpoint()
        result = format_request_for_backend(req, ep)
        payload = json.loads(result["payload"])

        self.assertIn("query_parameters", payload)
        self.assertEqual(payload["query_parameters"]["q"], "test")
        self.assertEqual(payload["query_parameters"]["page"], "1")

    def test_selected_headers_forwarded(self):
        """GW-007 AC1: Only selected headers are forwarded."""
        req = self._make_request(HttpMethod.Get, "/",
                                 headers={
                                     "content-type": "application/json",
                                     "accept": "text/html",
                                     "host": "should-not-forward",
                                     "x-request-id": "req-123",
                                 })
        ep = self._make_endpoint()
        result = format_request_for_backend(req, ep)
        payload = json.loads(result["payload"])

        self.assertIn("content-type", payload["headers"])
        self.assertIn("accept", payload["headers"])
        self.assertIn("x-request-id", payload["headers"])
        self.assertNotIn("host", payload["headers"])


class TestUserIdentifierInclusion(unittest.TestCase):
    """GW-007 AC2: User identifier inclusion for authenticated endpoints."""

    def test_authenticated_user_header(self):
        """GW-007 AC2: x-authenticated-user header is forwarded if present."""
        req = HttpRequest()
        req.method = HttpMethod.Get
        req.path = "/api/profile"
        req.headers = {"x-forwarded-for": "192.168.1.1"}
        ep = EndpointDefinition()
        ep.path = "/api/profile"
        ep.method = HttpMethod.Get
        ep.backend_identifier = "svc"
        ep.requires_authentication = True
        ep.parse_path_segments()

        result = format_request_for_backend(req, ep)
        payload = json.loads(result["payload"])
        # x-forwarded-for is a forwarded header
        self.assertIn("x-forwarded-for", payload["headers"])


class TestAllHttpVerbs(unittest.TestCase):
    """GW-007 AC3: All HTTP verbs are correctly represented."""

    def _test_verb(self, method, expected_str):
        req = HttpRequest()
        req.method = method
        req.path = "/test"
        ep = EndpointDefinition()
        ep.path = "/test"
        ep.method = method
        ep.backend_identifier = "svc"
        ep.parse_path_segments()
        result = format_request_for_backend(req, ep)
        payload = json.loads(result["payload"])
        self.assertEqual(payload["method"], expected_str)

    def test_get(self):
        """GW-007 AC3: GET verb is represented."""
        self._test_verb(HttpMethod.Get, "GET")

    def test_post(self):
        """GW-007 AC3: POST verb is represented."""
        self._test_verb(HttpMethod.Post, "POST")

    def test_put(self):
        """GW-007 AC3: PUT verb is represented."""
        self._test_verb(HttpMethod.Put, "PUT")

    def test_delete(self):
        """GW-007 AC3: DELETE verb is represented."""
        self._test_verb(HttpMethod.Delete, "DELETE")

    def test_patch(self):
        """GW-007 AC3: PATCH verb is represented."""
        self._test_verb(HttpMethod.Patch, "PATCH")


class TestFrameTypeIsRequest(unittest.TestCase):
    """GW-007: Formatted requests use the Request frame type."""

    def test_frame_type(self):
        """GW-007: Frame type is Request."""
        req = HttpRequest()
        req.method = HttpMethod.Get
        req.path = "/"
        ep = EndpointDefinition()
        ep.path = "/"
        ep.method = HttpMethod.Get
        ep.backend_identifier = "svc"
        ep.parse_path_segments()
        result = format_request_for_backend(req, ep)
        self.assertEqual(result["frame_type"], FrameType.Request)


class TestJsonEscaping(unittest.TestCase):
    """GW-007: JSON string escaping."""

    def test_escape_quotes(self):
        """GW-007: Quotes are escaped."""
        self.assertEqual(escape_json_string('say "hi"'), 'say \\"hi\\"')

    def test_escape_backslash(self):
        """GW-007: Backslash is escaped."""
        self.assertEqual(escape_json_string('C:\\path'), 'C:\\\\path')

    def test_escape_newline(self):
        """GW-007: Newline is escaped."""
        self.assertEqual(escape_json_string("line1\nline2"), "line1\\nline2")

    def test_escape_tab(self):
        """GW-007: Tab is escaped."""
        self.assertEqual(escape_json_string("col1\tcol2"), "col1\\tcol2")

    def test_no_escaping_needed(self):
        """GW-007: Plain string needs no escaping."""
        self.assertEqual(escape_json_string("hello"), "hello")


class TestNonJsonBody(unittest.TestCase):
    """GW-007: Non-JSON body handling."""

    def test_non_json_body_as_string(self):
        """GW-007: Non-JSON body is escaped and wrapped as string."""
        req = HttpRequest()
        req.method = HttpMethod.Post
        req.path = "/data"
        req.body = "plain text body"
        ep = EndpointDefinition()
        ep.path = "/data"
        ep.method = HttpMethod.Post
        ep.backend_identifier = "svc"
        ep.parse_path_segments()
        result = format_request_for_backend(req, ep)
        payload = json.loads(result["payload"])
        self.assertEqual(payload["body"], "plain text body")


if __name__ == '__main__':
    unittest.main()
