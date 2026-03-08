#!/usr/bin/env python3
"""
Backend Simulator for the Dynamic REST API Gateway Server.

Connects to the gateway's Unix domain socket, registers sample endpoints,
and handles incoming request frames by sending appropriate response frames.

Frame Format (big-endian):
  [4 bytes frame_type][4 bytes request_id][4 bytes payload_length][N bytes payload]

FrameType enum values:
  Registration  = 0
  Unregistration = 1
  Request       = 2
  Response      = 3
  LoginResponse = 4
  TokenResponse = 5
  Heartbeat     = 6
  Error         = 7
"""

import socket
import struct
import json
import sys
import os
import time
import threading
import argparse
import logging

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

FRAME_HEADER_SIZE = 12
MAX_PAYLOAD_SIZE = 1048576

FRAME_TYPE_REGISTRATION   = 0
FRAME_TYPE_UNREGISTRATION = 1
FRAME_TYPE_REQUEST        = 2
FRAME_TYPE_RESPONSE       = 3
FRAME_TYPE_LOGIN_RESPONSE = 4
FRAME_TYPE_TOKEN_RESPONSE = 5
FRAME_TYPE_HEARTBEAT      = 6
FRAME_TYPE_ERROR          = 7

# ---------------------------------------------------------------------------
# Logging setup
# ---------------------------------------------------------------------------

logging.basicConfig(
    level=logging.DEBUG,
    format="[%(asctime)s] %(levelname)-8s %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
)
logger = logging.getLogger("BackendSimulator")

# ---------------------------------------------------------------------------
# Frame helpers
# ---------------------------------------------------------------------------

def build_frame(frame_type, request_id, payload):
    """Serialize a frame into bytes."""
    if isinstance(payload, str):
        payload = payload.encode("utf-8")
    payload_len = len(payload)
    header = struct.pack("!III", frame_type, request_id, payload_len)
    return header + payload


def read_exact(sock, n):
    """Read exactly *n* bytes from a socket."""
    buf = b""
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("Connection closed while reading")
        buf += chunk
    return buf


def read_frame(sock):
    """Read one frame from *sock*.  Returns (frame_type, request_id, payload_str)."""
    header = read_exact(sock, FRAME_HEADER_SIZE)
    frame_type, request_id, payload_len = struct.unpack("!III", header)
    if payload_len > MAX_PAYLOAD_SIZE:
        raise ValueError(f"Payload too large: {payload_len}")
    payload = b""
    if payload_len > 0:
        payload = read_exact(sock, payload_len)
    return frame_type, request_id, payload.decode("utf-8")


def send_frame(sock, frame_type, request_id, payload):
    """Send a complete frame on *sock*."""
    data = build_frame(frame_type, request_id, payload)
    sock.sendall(data)

# ---------------------------------------------------------------------------
# Endpoint definitions (matching C++ EndpointDefinition JSON format)
# ---------------------------------------------------------------------------

BACKEND_ID = "test_backend_v1"

ENDPOINTS = [
    {
        "path": "/api/v1/health",
        "method": "GET",
        "description": "Health check endpoint",
        "requires_auth": False,
        "parameters": [],
    },
    {
        "path": "/api/v1/login",
        "method": "POST",
        "description": "User login endpoint",
        "requires_auth": False,
        "parameters": [
            {
                "name": "username",
                "type": "string",
                "location": "body",
                "required": True,
                "description": "Username for authentication",
            },
            {
                "name": "password",
                "type": "string",
                "location": "body",
                "required": True,
                "description": "Password for authentication",
            },
        ],
    },
    {
        "path": "/api/v1/users/{id}",
        "method": "GET",
        "description": "Get user by ID",
        "requires_auth": True,
        "parameters": [
            {
                "name": "id",
                "type": "integer",
                "location": "path",
                "required": True,
                "description": "User ID",
                "constraints": {"min_value": 1},
            },
        ],
    },
    {
        "path": "/api/v1/users",
        "method": "POST",
        "description": "Create a new user",
        "requires_auth": True,
        "parameters": [
            {
                "name": "name",
                "type": "string",
                "location": "body",
                "required": True,
                "description": "User full name",
                "constraints": {"min_length": 1, "max_length": 128},
            },
            {
                "name": "email",
                "type": "string",
                "location": "body",
                "required": True,
                "description": "User email address",
            },
            {
                "name": "age",
                "type": "integer",
                "location": "body",
                "required": False,
                "description": "User age",
                "constraints": {"min_value": 0, "max_value": 200},
            },
        ],
    },
    {
        "path": "/api/v1/users/{id}",
        "method": "PUT",
        "description": "Update a user",
        "requires_auth": True,
        "parameters": [
            {
                "name": "id",
                "type": "integer",
                "location": "path",
                "required": True,
                "description": "User ID",
                "constraints": {"min_value": 1},
            },
        ],
    },
    {
        "path": "/api/v1/users/{id}",
        "method": "DELETE",
        "description": "Delete a user",
        "requires_auth": True,
        "parameters": [
            {
                "name": "id",
                "type": "integer",
                "location": "path",
                "required": True,
                "description": "User ID",
                "constraints": {"min_value": 1},
            },
        ],
    },
    {
        "path": "/api/v1/orders",
        "method": "GET",
        "description": "List orders",
        "requires_auth": True,
        "parameters": [
            {
                "name": "page",
                "type": "integer",
                "location": "query",
                "required": False,
                "description": "Page number",
                "default": "1",
                "constraints": {"min_value": 1},
            },
            {
                "name": "limit",
                "type": "integer",
                "location": "query",
                "required": False,
                "description": "Items per page",
                "default": "20",
                "constraints": {"min_value": 1, "max_value": 100},
            },
        ],
    },
    {
        "path": "/api/v1/orders/{orderId}",
        "method": "GET",
        "description": "Get order by ID",
        "requires_auth": True,
        "parameters": [
            {
                "name": "orderId",
                "type": "integer",
                "location": "path",
                "required": True,
                "description": "Order ID",
                "constraints": {"min_value": 1},
            },
        ],
    },
    {
        "path": "/auth/refresh",
        "method": "POST",
        "description": "Refresh authentication tokens",
        "requires_auth": False,
        "parameters": [
            {
                "name": "refresh_token",
                "type": "string",
                "location": "body",
                "required": True,
                "description": "Refresh token",
            },
        ],
    },
]

# ---------------------------------------------------------------------------
# Sample data used in responses
# ---------------------------------------------------------------------------

SAMPLE_USERS = {
    1: {"id": 1, "name": "Alice Johnson", "email": "alice@example.com", "age": 30},
    2: {"id": 2, "name": "Bob Smith", "email": "bob@example.com", "age": 25},
    3: {"id": 3, "name": "Charlie Brown", "email": "charlie@example.com", "age": 35},
}

SAMPLE_ORDERS = {
    1: {"id": 1, "product": "Widget A", "quantity": 3, "total": 29.97, "user_id": 1},
    2: {"id": 2, "product": "Gadget B", "quantity": 1, "total": 49.99, "user_id": 2},
    3: {"id": 3, "product": "Thingamajig", "quantity": 5, "total": 24.95, "user_id": 1},
}

VALID_CREDENTIALS = {
    "admin": "admin123",
    "user1": "password1",
    "testuser": "testpass",
}

_next_user_id = 100

# ---------------------------------------------------------------------------
# Request handlers
# ---------------------------------------------------------------------------

def handle_health(request_data):
    """GET /api/v1/health"""
    return {
        "status_code": 200,
        "body": {
            "status": "healthy",
            "service": "test_backend_v1",
            "uptime_seconds": int(time.time() - _start_time),
        },
    }


def handle_login(request_data):
    """
    POST /api/v1/login

    This endpoint is special: the backend must return a LoginResponse frame
    (frame type 4) with {"success": true/false, "user_id": "..."} so the
    gateway can generate tokens.
    """
    body = request_data.get("body", {})
    username = body.get("username", "")
    password = body.get("password", "")

    if username in VALID_CREDENTIALS and VALID_CREDENTIALS[username] == password:
        return {
            "_frame_type": FRAME_TYPE_LOGIN_RESPONSE,
            "success": True,
            "user_id": username,
            "server_id": BACKEND_ID,
        }
    else:
        return {
            "_frame_type": FRAME_TYPE_LOGIN_RESPONSE,
            "success": False,
            "message": "Invalid username or password",
        }


def handle_get_user(request_data):
    """GET /api/v1/users/{id}"""
    path_params = request_data.get("path_parameters", {})
    user_id_str = path_params.get("id", "0")
    try:
        user_id = int(user_id_str)
    except ValueError:
        return {"status_code": 400, "body": {"error": "Invalid user ID"}}

    user = SAMPLE_USERS.get(user_id)
    if user:
        return {"status_code": 200, "body": user}
    else:
        return {"status_code": 404, "body": {"error": "User not found", "id": user_id}}


def handle_create_user(request_data):
    """POST /api/v1/users"""
    global _next_user_id
    body = request_data.get("body", {})
    name = body.get("name", "")
    email = body.get("email", "")
    age = body.get("age", None)

    new_user = {
        "id": _next_user_id,
        "name": name,
        "email": email,
    }
    if age is not None:
        new_user["age"] = age
    SAMPLE_USERS[_next_user_id] = new_user
    _next_user_id += 1

    return {"status_code": 201, "body": new_user}


def handle_update_user(request_data):
    """PUT /api/v1/users/{id}"""
    path_params = request_data.get("path_parameters", {})
    user_id_str = path_params.get("id", "0")
    try:
        user_id = int(user_id_str)
    except ValueError:
        return {"status_code": 400, "body": {"error": "Invalid user ID"}}

    user = SAMPLE_USERS.get(user_id)
    if not user:
        return {"status_code": 404, "body": {"error": "User not found", "id": user_id}}

    body = request_data.get("body", {})
    if isinstance(body, dict):
        for key, value in body.items():
            if key != "id":
                user[key] = value
    SAMPLE_USERS[user_id] = user
    return {"status_code": 200, "body": user}


def handle_delete_user(request_data):
    """DELETE /api/v1/users/{id}"""
    path_params = request_data.get("path_parameters", {})
    user_id_str = path_params.get("id", "0")
    try:
        user_id = int(user_id_str)
    except ValueError:
        return {"status_code": 400, "body": {"error": "Invalid user ID"}}

    if user_id in SAMPLE_USERS:
        del SAMPLE_USERS[user_id]
        return {"status_code": 200, "body": {"message": "User deleted", "id": user_id}}
    else:
        return {"status_code": 404, "body": {"error": "User not found", "id": user_id}}


def handle_list_orders(request_data):
    """GET /api/v1/orders"""
    query_params = request_data.get("query_parameters", {})
    page = int(query_params.get("page", "1"))
    limit = int(query_params.get("limit", "20"))

    all_orders = list(SAMPLE_ORDERS.values())
    start = (page - 1) * limit
    end = start + limit
    page_orders = all_orders[start:end]

    return {
        "status_code": 200,
        "body": {
            "orders": page_orders,
            "page": page,
            "limit": limit,
            "total": len(all_orders),
        },
    }


def handle_get_order(request_data):
    """GET /api/v1/orders/{orderId}"""
    path_params = request_data.get("path_parameters", {})
    order_id_str = path_params.get("orderId", "0")
    try:
        order_id = int(order_id_str)
    except ValueError:
        return {"status_code": 400, "body": {"error": "Invalid order ID"}}

    order = SAMPLE_ORDERS.get(order_id)
    if order:
        return {"status_code": 200, "body": order}
    else:
        return {"status_code": 404, "body": {"error": "Order not found", "id": order_id}}


# Route map: (method, path_template) -> handler
ROUTE_HANDLERS = {
    ("GET",    "/api/v1/health"):           handle_health,
    ("POST",   "/api/v1/login"):            handle_login,
    ("GET",    "/api/v1/users/{id}"):        handle_get_user,
    ("POST",   "/api/v1/users"):             handle_create_user,
    ("PUT",    "/api/v1/users/{id}"):        handle_update_user,
    ("DELETE", "/api/v1/users/{id}"):        handle_delete_user,
    ("GET",    "/api/v1/orders"):            handle_list_orders,
    ("GET",    "/api/v1/orders/{orderId}"):  handle_get_order,
}


def _match_route(method, path):
    """
    Find a handler by matching the request method and path against registered
    endpoint templates.  Returns (handler, True) or (None, False).
    """
    # Try an exact match first
    handler = ROUTE_HANDLERS.get((method, path))
    if handler:
        return handler

    # Try template matching (paths with parameters)
    for (m, template), h in ROUTE_HANDLERS.items():
        if m != method:
            continue
        tmpl_segs = [s for s in template.split("/") if s]
        path_segs = [s for s in path.split("/") if s]
        if len(tmpl_segs) != len(path_segs):
            continue
        match = True
        for t, p in zip(tmpl_segs, path_segs):
            if t.startswith("{") and t.endswith("}"):
                continue
            if t != p:
                match = False
                break
        if match:
            return h
    return None


def dispatch_request(request_data):
    """Dispatch a parsed request JSON to the appropriate handler."""
    method = request_data.get("method", "GET")
    path = request_data.get("path", "/")

    handler = _match_route(method, path)
    if handler:
        return handler(request_data)
    else:
        return {
            "status_code": 404,
            "body": {"error": "No handler for route", "method": method, "path": path},
        }

# ---------------------------------------------------------------------------
# Main loop
# ---------------------------------------------------------------------------

_start_time = time.time()


class BackendSimulator:
    """
    Manages a single Unix-domain-socket connection to the gateway, registering
    endpoints and handling forwarded requests.
    """

    def __init__(self, socket_path, backend_id=BACKEND_ID):
        self.socket_path = socket_path
        self.backend_id = backend_id
        self.sock = None
        self.running = False
        self.request_count = 0

    def connect(self):
        """Connect to the gateway's Unix domain socket."""
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.connect(self.socket_path)
        logger.info("Connected to gateway at %s", self.socket_path)

    def register(self):
        """Send the registration frame."""
        registration_payload = json.dumps({
            "backend_id": self.backend_id,
            "endpoints": ENDPOINTS,
        })
        send_frame(self.sock, FRAME_TYPE_REGISTRATION, 0, registration_payload)
        logger.info("Sent registration frame for backend '%s' with %d endpoints",
                     self.backend_id, len(ENDPOINTS))

        # Wait for acknowledgement
        frame_type, req_id, payload = read_frame(self.sock)
        if frame_type == FRAME_TYPE_RESPONSE:
            ack = json.loads(payload)
            logger.info("Registration acknowledged: %s", ack)
        else:
            logger.warning("Unexpected frame type %d after registration", frame_type)

    def unregister(self):
        """Send the unregistration frame."""
        payload = json.dumps({"backend_id": self.backend_id})
        send_frame(self.sock, FRAME_TYPE_UNREGISTRATION, 0, payload)
        logger.info("Sent unregistration frame for backend '%s'", self.backend_id)

    def close(self):
        """Close the socket connection."""
        if self.sock:
            try:
                self.unregister()
            except Exception:
                pass
            self.sock.close()
            self.sock = None
            logger.info("Connection closed")

    def handle_request_frame(self, request_id, payload_str):
        """Process one Request frame and send the appropriate response."""
        self.request_count += 1
        try:
            request_data = json.loads(payload_str)
        except json.JSONDecodeError:
            logger.error("Invalid JSON in request payload: %s", payload_str[:200])
            error_resp = json.dumps({"status_code": 400, "body": {"error": "Bad request payload"}})
            send_frame(self.sock, FRAME_TYPE_RESPONSE, request_id, error_resp)
            return

        method = request_data.get("method", "?")
        path = request_data.get("path", "?")
        logger.info("Request #%d: %s %s (req_id=%d)", self.request_count, method, path, request_id)

        result = dispatch_request(request_data)

        # Check if this is a login response (special frame type)
        special_frame_type = result.pop("_frame_type", None)
        if special_frame_type == FRAME_TYPE_LOGIN_RESPONSE:
            response_payload = json.dumps(result)
            send_frame(self.sock, FRAME_TYPE_LOGIN_RESPONSE, request_id, response_payload)
            logger.info("  -> LoginResponse: success=%s", result.get("success"))

            # If login succeeded, we expect the gateway to send us a TokenResponse frame
            if result.get("success"):
                try:
                    ft, rid, tp = read_frame(self.sock)
                    if ft == FRAME_TYPE_TOKEN_RESPONSE:
                        token_data = json.loads(tp)
                        logger.info("  -> Received TokenResponse for user '%s'",
                                    token_data.get("user_id", "?"))
                    else:
                        logger.warning("  -> Expected TokenResponse, got frame type %d", ft)
                except Exception as exc:
                    logger.warning("  -> Error reading TokenResponse: %s", exc)
        else:
            response_payload = json.dumps(result)
            send_frame(self.sock, FRAME_TYPE_RESPONSE, request_id, response_payload)
            logger.info("  -> Response: status=%d", result.get("status_code", 200))

    def run(self):
        """Main loop: read frames from the gateway and handle them."""
        self.running = True
        logger.info("Backend simulator running, waiting for requests...")

        while self.running:
            try:
                frame_type, request_id, payload = read_frame(self.sock)

                if frame_type == FRAME_TYPE_REQUEST:
                    self.handle_request_frame(request_id, payload)
                elif frame_type == FRAME_TYPE_HEARTBEAT:
                    logger.debug("Heartbeat received (req_id=%d)", request_id)
                    hb_resp = json.dumps({"status": "alive"})
                    send_frame(self.sock, FRAME_TYPE_HEARTBEAT, request_id, hb_resp)
                elif frame_type == FRAME_TYPE_ERROR:
                    logger.error("Error frame received: %s", payload)
                    self.running = False
                else:
                    logger.warning("Unexpected frame type %d, payload: %s",
                                   frame_type, payload[:200])

            except ConnectionError:
                logger.info("Connection closed by gateway")
                self.running = False
            except Exception as exc:
                logger.error("Error in main loop: %s", exc, exc_info=True)
                self.running = False

        logger.info("Backend simulator stopped after handling %d requests", self.request_count)


def main():
    parser = argparse.ArgumentParser(description="Backend Simulator for Gateway Integration Tests")
    parser.add_argument("--socket", "-s", default="/tmp/gateway.sock",
                        help="Unix domain socket path (default: /tmp/gateway.sock)")
    parser.add_argument("--backend-id", "-b", default=BACKEND_ID,
                        help=f"Backend identifier (default: {BACKEND_ID})")
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="Enable debug logging")
    args = parser.parse_args()

    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)

    simulator = BackendSimulator(args.socket, args.backend_id)
    try:
        simulator.connect()
        simulator.register()
        simulator.run()
    except KeyboardInterrupt:
        logger.info("Interrupted by user")
    except ConnectionRefusedError:
        logger.error("Cannot connect to gateway at %s - is it running?", args.socket)
        sys.exit(1)
    except FileNotFoundError:
        logger.error("Socket file not found: %s - is the gateway running?", args.socket)
        sys.exit(1)
    finally:
        simulator.close()


if __name__ == "__main__":
    main()
