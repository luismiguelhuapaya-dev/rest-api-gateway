"""
Unit tests for the routing table.
Tests mirror the behaviour implemented in src/routing/RoutingTable.cpp
and src/routing/EndpointDefinition.cpp.

Reference: GW-004 - Dynamic Route Registration
"""

import unittest
import sys
import os

sys.path.insert(0, os.path.dirname(__file__))
from test_helpers import (
    EndpointDefinition, HttpMethod, http_method_to_string
)


# ---------------------------------------------------------------------------
# Python reference routing table mirroring RoutingTable.cpp
# ---------------------------------------------------------------------------

MAX_ENDPOINTS = 4096


class RoutingTable:
    """Python reference implementation of the C++ RoutingTable."""

    def __init__(self):
        self._endpoints: list = []
        self._route_index: dict = {}

    def register_endpoint(self, defn: EndpointDefinition) -> bool:
        if not defn.is_valid():
            return False
        route_key = defn.get_route_key()
        if route_key in self._route_index:
            # Update existing
            idx = self._route_index[route_key]
            self._endpoints[idx] = defn
            return True
        if len(self._endpoints) >= MAX_ENDPOINTS:
            return False
        idx = len(self._endpoints)
        self._endpoints.append(defn)
        self._route_index[route_key] = idx
        return True

    def unregister_endpoint(self, path: str, method: HttpMethod) -> bool:
        route_key = http_method_to_string(method) + ":" + path
        if route_key not in self._route_index:
            return False
        idx = self._route_index[route_key]
        # Swap with last and pop
        if idx < len(self._endpoints) - 1:
            last = self._endpoints[-1]
            last_key = last.get_route_key()
            self._endpoints[idx] = last
            self._route_index[last_key] = idx
        self._endpoints.pop()
        del self._route_index[route_key]
        return True

    def find_endpoint(self, path: str, method: HttpMethod):
        """Returns (found, EndpointDefinition, path_params)."""
        route_key = http_method_to_string(method) + ":" + path
        if route_key in self._route_index:
            return True, self._endpoints[self._route_index[route_key]], {}
        # Pattern matching
        for ep in self._endpoints:
            if ep.method == method:
                matched, params = ep.matches_path(path)
                if matched:
                    return True, ep, params
        return False, None, {}

    def has_endpoint(self, path: str, method: HttpMethod) -> bool:
        found, _, _ = self.find_endpoint(path, method)
        return found

    def get_endpoint_count(self) -> int:
        return len(self._endpoints)

    def get_all_endpoints(self):
        return list(self._endpoints)

    def get_endpoints_by_backend(self, backend_id: str):
        return [ep for ep in self._endpoints if ep.backend_identifier == backend_id]

    def remove_endpoints_by_backend(self, backend_id: str) -> int:
        new_eps = []
        new_index = {}
        removed = 0
        for ep in self._endpoints:
            if ep.backend_identifier != backend_id:
                new_index[ep.get_route_key()] = len(new_eps)
                new_eps.append(ep)
            else:
                removed += 1
        self._endpoints = new_eps
        self._route_index = new_index
        return removed

    def clear(self):
        self._endpoints.clear()
        self._route_index.clear()


def _make_endpoint(path, method, backend, auth=False):
    ep = EndpointDefinition()
    ep.path = path
    ep.method = method
    ep.backend_identifier = backend
    ep.requires_authentication = auth
    ep.parse_path_segments()
    return ep


# ---------------------------------------------------------------------------
# Test Cases
# ---------------------------------------------------------------------------

class TestExactPathMatching(unittest.TestCase):
    """GW-004 AC1: Exact path matching."""

    def test_register_and_find_exact(self):
        """GW-004 AC1: Register an exact path and find it."""
        rt = RoutingTable()
        ep = _make_endpoint("/api/users", HttpMethod.Get, "user-service")
        self.assertTrue(rt.register_endpoint(ep))
        found, result, params = rt.find_endpoint("/api/users", HttpMethod.Get)
        self.assertTrue(found)
        self.assertEqual(result.path, "/api/users")
        self.assertEqual(result.method, HttpMethod.Get)
        self.assertEqual(params, {})

    def test_different_path_not_found(self):
        """GW-004 AC1: Different path returns not found."""
        rt = RoutingTable()
        ep = _make_endpoint("/api/users", HttpMethod.Get, "user-service")
        rt.register_endpoint(ep)
        found, _, _ = rt.find_endpoint("/api/products", HttpMethod.Get)
        self.assertFalse(found)


class TestParameterizedPathMatching(unittest.TestCase):
    """GW-004 AC2: Parameterized path matching (/users/{id})."""

    def test_single_parameter(self):
        """GW-004 AC2: Path with one {param} extracts parameter."""
        rt = RoutingTable()
        ep = _make_endpoint("/api/users/{id}", HttpMethod.Get, "user-service")
        rt.register_endpoint(ep)

        found, _, params = rt.find_endpoint("/api/users/42", HttpMethod.Get)
        self.assertTrue(found)
        self.assertEqual(params, {"id": "42"})

    def test_multiple_parameters(self):
        """GW-004 AC2: Path with multiple {params} extracts all."""
        rt = RoutingTable()
        ep = _make_endpoint("/api/users/{userId}/posts/{postId}", HttpMethod.Get, "post-service")
        rt.register_endpoint(ep)

        found, _, params = rt.find_endpoint("/api/users/7/posts/99", HttpMethod.Get)
        self.assertTrue(found)
        self.assertEqual(params, {"userId": "7", "postId": "99"})

    def test_parameter_mismatch_segment_count(self):
        """GW-004 AC2: Different number of segments does not match."""
        rt = RoutingTable()
        ep = _make_endpoint("/api/users/{id}", HttpMethod.Get, "user-service")
        rt.register_endpoint(ep)

        found, _, _ = rt.find_endpoint("/api/users/42/extra", HttpMethod.Get)
        self.assertFalse(found)

    def test_literal_segment_mismatch(self):
        """GW-004 AC2: Literal segment mismatch fails."""
        rt = RoutingTable()
        ep = _make_endpoint("/api/users/{id}", HttpMethod.Get, "user-service")
        rt.register_endpoint(ep)

        found, _, _ = rt.find_endpoint("/api/products/42", HttpMethod.Get)
        self.assertFalse(found)


class TestVerbPathCombination(unittest.TestCase):
    """GW-004 AC3: Verb + path combination matching."""

    def test_same_path_different_verbs(self):
        """GW-004 AC3: Same path with different HTTP methods are separate endpoints."""
        rt = RoutingTable()
        ep_get = _make_endpoint("/api/users", HttpMethod.Get, "user-service")
        ep_post = _make_endpoint("/api/users", HttpMethod.Post, "user-service")
        rt.register_endpoint(ep_get)
        rt.register_endpoint(ep_post)

        found_get, res_get, _ = rt.find_endpoint("/api/users", HttpMethod.Get)
        found_post, res_post, _ = rt.find_endpoint("/api/users", HttpMethod.Post)

        self.assertTrue(found_get)
        self.assertTrue(found_post)
        self.assertEqual(res_get.method, HttpMethod.Get)
        self.assertEqual(res_post.method, HttpMethod.Post)

    def test_wrong_verb_not_found(self):
        """GW-004 AC3: Wrong verb for existing path returns not found."""
        rt = RoutingTable()
        ep = _make_endpoint("/api/users", HttpMethod.Get, "user-service")
        rt.register_endpoint(ep)

        found, _, _ = rt.find_endpoint("/api/users", HttpMethod.Delete)
        self.assertFalse(found)


class TestRegistrationDeregistration(unittest.TestCase):
    """GW-004 AC4: Registration and deregistration."""

    def test_register_and_count(self):
        """GW-004 AC4: Registering endpoints increases count."""
        rt = RoutingTable()
        self.assertEqual(rt.get_endpoint_count(), 0)
        rt.register_endpoint(_make_endpoint("/a", HttpMethod.Get, "svc"))
        self.assertEqual(rt.get_endpoint_count(), 1)
        rt.register_endpoint(_make_endpoint("/b", HttpMethod.Get, "svc"))
        self.assertEqual(rt.get_endpoint_count(), 2)

    def test_unregister(self):
        """GW-004 AC4: Unregistering removes the endpoint."""
        rt = RoutingTable()
        rt.register_endpoint(_make_endpoint("/a", HttpMethod.Get, "svc"))
        self.assertTrue(rt.has_endpoint("/a", HttpMethod.Get))
        self.assertTrue(rt.unregister_endpoint("/a", HttpMethod.Get))
        self.assertFalse(rt.has_endpoint("/a", HttpMethod.Get))
        self.assertEqual(rt.get_endpoint_count(), 0)

    def test_unregister_nonexistent(self):
        """GW-004 AC4: Unregistering a nonexistent endpoint returns False."""
        rt = RoutingTable()
        self.assertFalse(rt.unregister_endpoint("/nope", HttpMethod.Get))

    def test_clear(self):
        """GW-004 AC4: Clear removes all endpoints."""
        rt = RoutingTable()
        rt.register_endpoint(_make_endpoint("/a", HttpMethod.Get, "svc"))
        rt.register_endpoint(_make_endpoint("/b", HttpMethod.Post, "svc"))
        rt.clear()
        self.assertEqual(rt.get_endpoint_count(), 0)


class TestDuplicateEndpoint(unittest.TestCase):
    """GW-004 AC5: Duplicate endpoint handling."""

    def test_duplicate_updates(self):
        """GW-004 AC5: Re-registering the same route key updates the endpoint."""
        rt = RoutingTable()
        ep1 = _make_endpoint("/api/users", HttpMethod.Get, "service-v1")
        ep2 = _make_endpoint("/api/users", HttpMethod.Get, "service-v2")
        rt.register_endpoint(ep1)
        rt.register_endpoint(ep2)
        # Should still be 1 endpoint (updated, not duplicated)
        self.assertEqual(rt.get_endpoint_count(), 1)
        _, result, _ = rt.find_endpoint("/api/users", HttpMethod.Get)
        self.assertEqual(result.backend_identifier, "service-v2")


class TestMultipleBackends(unittest.TestCase):
    """GW-004 AC6: Multiple backends under same Server ID."""

    def test_multiple_endpoints_same_backend(self):
        """GW-004 AC6: One backend can register multiple endpoints."""
        rt = RoutingTable()
        rt.register_endpoint(_make_endpoint("/api/users", HttpMethod.Get, "user-service"))
        rt.register_endpoint(_make_endpoint("/api/users/{id}", HttpMethod.Get, "user-service"))
        rt.register_endpoint(_make_endpoint("/api/users", HttpMethod.Post, "user-service"))

        by_backend = rt.get_endpoints_by_backend("user-service")
        self.assertEqual(len(by_backend), 3)

    def test_remove_by_backend(self):
        """GW-004 AC6: RemoveEndpointsByBackend removes all for that backend."""
        rt = RoutingTable()
        rt.register_endpoint(_make_endpoint("/a", HttpMethod.Get, "svc-a"))
        rt.register_endpoint(_make_endpoint("/b", HttpMethod.Get, "svc-a"))
        rt.register_endpoint(_make_endpoint("/c", HttpMethod.Get, "svc-b"))

        removed = rt.remove_endpoints_by_backend("svc-a")
        self.assertEqual(removed, 2)
        self.assertEqual(rt.get_endpoint_count(), 1)
        self.assertTrue(rt.has_endpoint("/c", HttpMethod.Get))
        self.assertFalse(rt.has_endpoint("/a", HttpMethod.Get))


class TestUnknownRoute(unittest.TestCase):
    """GW-004 AC7: Unknown route returns 404 indication."""

    def test_unknown_route_not_found(self):
        """GW-004 AC7: Finding a non-registered route returns False."""
        rt = RoutingTable()
        rt.register_endpoint(_make_endpoint("/api/users", HttpMethod.Get, "svc"))
        found, _, _ = rt.find_endpoint("/api/unknown", HttpMethod.Get)
        self.assertFalse(found)

    def test_has_endpoint_false_for_unknown(self):
        """GW-004 AC7: has_endpoint returns False for unknown paths."""
        rt = RoutingTable()
        self.assertFalse(rt.has_endpoint("/nothing", HttpMethod.Get))


class TestInvalidEndpoint(unittest.TestCase):
    """GW-004: Invalid endpoint definitions are rejected."""

    def test_empty_path_rejected(self):
        """GW-004: Endpoint with empty path is rejected."""
        rt = RoutingTable()
        ep = EndpointDefinition()
        ep.path = ""
        ep.method = HttpMethod.Get
        ep.backend_identifier = "svc"
        ep.parse_path_segments()
        self.assertFalse(rt.register_endpoint(ep))

    def test_unknown_method_rejected(self):
        """GW-004: Endpoint with Unknown method is rejected."""
        rt = RoutingTable()
        ep = EndpointDefinition()
        ep.path = "/test"
        ep.method = HttpMethod.Unknown
        ep.backend_identifier = "svc"
        ep.parse_path_segments()
        self.assertFalse(rt.register_endpoint(ep))

    def test_empty_backend_rejected(self):
        """GW-004: Endpoint with empty backend identifier is rejected."""
        rt = RoutingTable()
        ep = EndpointDefinition()
        ep.path = "/test"
        ep.method = HttpMethod.Get
        ep.backend_identifier = ""
        ep.parse_path_segments()
        self.assertFalse(rt.register_endpoint(ep))


class TestGetRouteKey(unittest.TestCase):
    """GW-004: Route key format."""

    def test_route_key_format(self):
        """GW-004: Route key is METHOD:path."""
        ep = _make_endpoint("/api/users", HttpMethod.Get, "svc")
        self.assertEqual(ep.get_route_key(), "GET:/api/users")

    def test_route_key_post(self):
        """GW-004: Route key for POST."""
        ep = _make_endpoint("/api/users", HttpMethod.Post, "svc")
        self.assertEqual(ep.get_route_key(), "POST:/api/users")


if __name__ == '__main__':
    unittest.main()
