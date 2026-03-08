# Dynamic REST API Gateway Server

A high-performance, zero-dependency API gateway written in pure C++20 that dynamically discovers and proxies REST endpoints registered by backend services at runtime. The gateway sits between HTTP clients (via NGINX reverse proxy) and backend services (via Unix domain sockets), providing schema-enforced request validation, AES-256-GCM encrypted token authentication, and coroutine-driven async I/O -- all without a single third-party library.

```
                    +-----------+
                    |  Clients  |
                    +-----+-----+
                          |  HTTPS
                    +-----+-----+
                    |   NGINX   |  (TLS termination)
                    +-----+-----+
                          |  HTTP
                +---------+---------+
                |    REST API       |
                |    Gateway        |   <-- this project
                | (epoll + C++20   |
                |  coroutines)      |
                +---------+---------+
                          |  Unix domain socket
          +---------------+---------------+
          |               |               |
    +-----+-----+  +-----+-----+  +------+----+
    | Backend A  |  | Backend B  |  | Backend C |
    | (any lang) |  | (any lang) |  | (any lang)|
    +------------+  +------------+  +-----------+
```

## Key Features

- **Zero built-in endpoints** -- every API route is dynamically registered by backend services at connection time. No gateway restarts needed.
- **Deep packet inspection and schema-enforced validation** -- backends declare parameter schemas (type, constraints, regex patterns) and the gateway validates every request before it reaches the backend.
- **AES-256-GCM encrypted self-describing tokens** -- authentication tokens carry their own payload (server ID, user ID, expiry) encrypted via the Linux kernel crypto API (`AF_ALG`). No JWT libraries, no shared databases.
- **C++20 coroutines with epoll** -- fully asynchronous, single-threaded I/O using `co_await` on file descriptor readiness. No thread pool overhead.
- **Hot reconfiguration** -- backends connect, register endpoints, and disconnect freely. The routing table updates atomically with no downtime.
- **No external dependencies** -- pure C++20 and Linux kernel APIs. No Boost, no OpenSSL, no libcurl, no libevent.

## Architecture Overview

The gateway operates as a six-stage request processing pipeline:

```
HTTP Request --> [1. Parse] --> [2. Route] --> [3. Authenticate] --> [4. Validate] --> [5. Forward] --> [6. Respond]
                    |              |                |                    |                |               |
                 HttpParser    RoutingTable    TokenEngine        ValidationEngine   BackendForwarder  HttpResponse
```

1. **Parse** -- Incoming HTTP bytes are parsed into a structured `HttpRequest` (method, path, headers, query params, body).
2. **Route** -- The `RoutingTable` matches the path and method against dynamically registered `EndpointDefinition` entries, extracting path parameters.
3. **Authenticate** -- If the endpoint requires auth, the `TokenEngine` decrypts and validates the Bearer token, checking server ID scope and expiry.
4. **Validate** -- The `ValidationEngine` checks every parameter (path, query, header, body) against the endpoint's declared `ParameterSchema`, enforcing types, ranges, patterns, and required fields.
5. **Forward** -- The `BackendForwarder` serializes the validated request into a binary frame and sends it to the correct backend over its Unix domain socket.
6. **Respond** -- The backend's response frame is deserialized and converted into an HTTP response sent back to the client.

For full architectural details, see [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## System Requirements

| Requirement | Minimum Version |
|---|---|
| Operating System | Linux (kernel 5.x+) |
| C++ Compiler | GCC 12+ or Clang 14+ |
| CMake | 3.20+ |
| Kernel features | `epoll`, `AF_ALG` (gcm(aes)), `getrandom()` |

> **Note:** This project uses Linux-specific APIs (`epoll`, `AF_ALG`, Unix domain sockets) and cannot be built on macOS or Windows.

## Build Instructions

```bash
# Clone the repository
git clone <repository-url>
cd rest-api-gateway

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# The binary is at build/rest-api-gateway
```

For debug builds:

```bash
mkdir build-debug && cd build-debug
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

## Configuration

The gateway accepts configuration through three sources, in order of precedence (highest to lowest):

1. **Command-line arguments** (override everything)
2. **JSON configuration file** (loaded via `-c`/`--config`)
3. **Environment variables** (loaded first, lowest precedence)

### Command-Line Arguments

```
Usage: rest-api-gateway [options]

Options:
  -c, --config <file>         Configuration file path
  -p, --port <port>           TCP listen port (default: 8080)
  -a, --address <addr>        TCP listen address (default: 0.0.0.0)
  -s, --socket <path>         Unix socket path (default: /tmp/gateway.sock)
  -m, --max-connections <n>   Max connections (default: 1024)
  --log-level <level>         Log level: debug|info|warning|error|fatal
  --log-file <path>           Log file path (default: /var/log/gateway.log)
  --log-stdout                Log to stdout (default: enabled)
  --no-log-stdout             Disable stdout logging
  --access-expiry <sec>       Access token expiry in seconds (default: 300)
  --refresh-expiry <sec>      Refresh token expiry in seconds (default: 86400)
```

### Environment Variables

| Variable | Description | Default |
|---|---|---|
| `GATEWAY_AES_KEY` | AES-256 key as a 64-character hex string | *(none -- auth disabled)* |
| `GATEWAY_PORT` | TCP listen port | `8080` |
| `GATEWAY_ADDRESS` | TCP listen address | `0.0.0.0` |
| `GATEWAY_SOCKET_PATH` | Unix domain socket path | `/tmp/gateway.sock` |
| `GATEWAY_LOG_LEVEL` | Log level (`debug`, `info`, `warning`, `error`, `fatal`) | `info` |

### JSON Configuration File

```json
{
    "listen_address": "127.0.0.1",
    "listen_port": 8080,
    "unix_socket_path": "/tmp/gateway.sock",
    "max_connections": 1024,
    "read_timeout_ms": 30000,
    "write_timeout_ms": 30000,
    "max_request_body_size": 1048576,
    "access_token_expiry_seconds": 300,
    "refresh_token_expiry_seconds": 86400,
    "log_level": "info",
    "log_file": "/var/log/gateway.log",
    "log_to_stdout": true,
    "aes_key": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
}
```

For a complete parameter reference, see [docs/CONFIGURATION.md](docs/CONFIGURATION.md).

## Running the Server

```bash
# Minimal startup (uses defaults, auth disabled)
./rest-api-gateway

# With a config file
./rest-api-gateway -c /etc/gateway/config.json

# With inline options
export GATEWAY_AES_KEY="$(openssl rand -hex 32)"
./rest-api-gateway -p 9000 -s /run/gateway.sock --log-level debug
```

The server will listen on two sockets simultaneously:

- **TCP socket** (default `0.0.0.0:8080`) -- for incoming HTTP client requests
- **Unix domain socket** (default `/tmp/gateway.sock`) -- for backend service connections

## Backend Integration Guide

Backend services connect to the gateway over a Unix domain socket and communicate using a binary frame protocol. Backends can be written in any language that supports Unix sockets.

### Connection Flow

```
Backend                                          Gateway
  |                                                |
  |--- connect(/tmp/gateway.sock) ---------------->|
  |                                                |
  |--- Registration Frame (endpoints JSON) ------->|
  |<-- Response Frame (ack) -----------------------|
  |                                                |
  |   ... gateway forwards client requests ...     |
  |                                                |
  |<-- Request Frame ------------------------------>|
  |--- Response Frame ----------------------------->|
  |                                                |
  |--- Unregistration Frame ----------------------->|  (graceful disconnect)
  |                                                |
```

### Frame Protocol

Each frame has a 12-byte header followed by a variable-length JSON payload:

```
Offset  Size     Field
------  ------   ----------------
0       4 bytes  Frame type (uint32, big-endian)
4       4 bytes  Request ID (uint32, big-endian)
8       4 bytes  Payload length (uint32, big-endian)
12      N bytes  JSON payload
```

Frame types:

| Value | Name | Direction | Description |
|---|---|---|---|
| 0 | Registration | Backend -> Gateway | Register backend and its endpoints |
| 1 | Unregistration | Backend -> Gateway | Graceful disconnect |
| 2 | Request | Gateway -> Backend | Forwarded client request |
| 3 | Response | Backend -> Gateway | Backend response to a request |
| 4 | LoginResponse | Backend -> Gateway | Authentication result |
| 5 | TokenResponse | Gateway -> Backend | Issued tokens (after login) |
| 6 | Heartbeat | Bidirectional | Keep-alive ping/pong |
| 7 | Error | Either | Error indication |

### Registration Frame Example

When a backend connects, it sends a Registration frame with a JSON payload describing its identity and endpoints:

```json
{
    "backend_id": "user-service",
    "endpoints": [
        {
            "path": "/api/users",
            "method": "GET",
            "description": "List all users",
            "requires_auth": true,
            "parameters": [
                {
                    "name": "page",
                    "type": "integer",
                    "location": "query",
                    "required": false,
                    "description": "Page number",
                    "constraints": {
                        "min_value": 1,
                        "max_value": 10000
                    }
                },
                {
                    "name": "limit",
                    "type": "integer",
                    "location": "query",
                    "required": false,
                    "default": "20",
                    "constraints": {
                        "min_value": 1,
                        "max_value": 100
                    }
                }
            ]
        },
        {
            "path": "/api/users/{user_id}",
            "method": "GET",
            "description": "Get a specific user",
            "requires_auth": true,
            "parameters": [
                {
                    "name": "user_id",
                    "type": "string",
                    "location": "path",
                    "required": true,
                    "constraints": {
                        "pattern": "^[a-f0-9]{24}$"
                    }
                }
            ]
        },
        {
            "path": "/api/users",
            "method": "POST",
            "description": "Create a new user",
            "requires_auth": true,
            "parameters": [
                {
                    "name": "username",
                    "type": "string",
                    "location": "body",
                    "required": true,
                    "constraints": {
                        "min_length": 3,
                        "max_length": 32,
                        "pattern": "^[a-zA-Z0-9_]+$"
                    }
                },
                {
                    "name": "email",
                    "type": "string",
                    "location": "body",
                    "required": true,
                    "constraints": {
                        "pattern": "^[^@]+@[^@]+\\.[^@]+$"
                    }
                },
                {
                    "name": "age",
                    "type": "integer",
                    "location": "body",
                    "required": false,
                    "constraints": {
                        "min_value": 0,
                        "max_value": 200
                    }
                }
            ]
        }
    ]
}
```

### Request Frame (Gateway -> Backend)

When a client request matches a registered endpoint, the gateway sends it to the backend as a JSON payload:

```json
{
    "method": "POST",
    "path": "/api/users",
    "backend": "user-service",
    "path_parameters": {},
    "query_parameters": {
        "notify": "true"
    },
    "headers": {
        "content-type": "application/json",
        "accept": "application/json",
        "x-forwarded-for": "192.168.1.100"
    },
    "body": {
        "username": "johndoe",
        "email": "john@example.com",
        "age": 30
    }
}
```

### Response Frame (Backend -> Gateway)

The backend responds with a JSON payload. The `status_code` field controls the HTTP status code returned to the client:

```json
{
    "status_code": 201,
    "body": {
        "id": "507f1f77bcf86cd799439011",
        "username": "johndoe",
        "email": "john@example.com",
        "created_at": "2025-01-15T10:30:00Z"
    }
}
```

### Login Flow (Two-Frame Exchange)

For authentication endpoints, the gateway orchestrates a two-frame exchange:

1. Gateway sends the login request to the backend as a normal Request frame.
2. Backend verifies credentials and returns a **LoginResponse** frame:

```json
{
    "success": true,
    "user_id": "user_12345",
    "server_id": "user-service"
}
```

3. If `success` is `true`, the gateway generates an AES-256-GCM encrypted token pair and sends a **TokenResponse** frame back to the backend (for session bookkeeping):

```json
{
    "access_token": "...",
    "refresh_token": "...",
    "user_id": "user_12345"
}
```

4. The gateway returns the token pair to the HTTP client.

For full protocol details, see [docs/PROTOCOL.md](docs/PROTOCOL.md).

## Authentication

### Token Format

Authentication tokens are AES-256-GCM encrypted binary payloads, base64url-encoded for transport. Each token is self-describing -- it carries all the information needed for validation without any database lookup:

```
Plaintext layout (before encryption):
[4 bytes: Server ID length][N bytes: Server ID string]
[4 bytes: User ID length][N bytes: User ID string]
[1 byte: Token type (0x00=access, 0x01=refresh)]
[8 bytes: Creation timestamp (int64, big-endian, Unix epoch)]
[8 bytes: Expiry timestamp (int64, big-endian, Unix epoch)]

Wire format:
[12 bytes: IV][encrypted payload][16 bytes: GCM auth tag]
 \___________________________________________/
              base64url encoded
```

### Token Lifecycle

| Step | Action |
|---|---|
| **Issuance** | Backend authenticates user, gateway generates access + refresh token pair |
| **Validation** | Gateway decrypts token, checks type, server ID scope, and expiry |
| **Refresh** | Client sends refresh token to `/auth/refresh`, gateway issues a new pair and revokes the old refresh token |
| **Revocation** | Old refresh tokens are added to an in-memory revocation set |

### Server ID Model

Each token is scoped to a `server_id` (typically the `backend_id`). A token issued for `user-service` cannot be used to access endpoints registered by `order-service`. This provides per-service isolation without any centralized authorization database.

### Setting Up Authentication

```bash
# Generate a 256-bit key
export GATEWAY_AES_KEY="$(openssl rand -hex 32)"

# Start the gateway
./rest-api-gateway -c config.json
```

When no AES key is configured, the gateway starts in degraded mode with authentication features disabled. Endpoints that declare `"requires_auth": true` will reject all requests with 401.

## API Validation

The gateway validates every request against the schema declared during endpoint registration. Validation covers four parameter locations: `path`, `query`, `header`, and `body`.

### Supported Data Types

| Type | Location | Validation |
|---|---|---|
| `string` | path, query, header, body | Length, regex pattern, allowed values |
| `integer` | path, query, header, body | Numeric format, min/max value |
| `float` | path, query, header, body | Numeric format, min/max value |
| `boolean` | path, query, header, body | Accepts `true`/`false`, `1`/`0`, `yes`/`no` |
| `object` | body | Type check (must be JSON object) |
| `array` | body | Type check (must be JSON array) |

### Error Response Format

When validation fails, the gateway returns a `400 Bad Request` with a structured JSON error body:

```json
{
    "errors": [
        {
            "parameter": "username",
            "message": "String length 2 is below minimum 3",
            "provided_value": "ab"
        },
        {
            "parameter": "email",
            "message": "String does not match required pattern"
        }
    ]
}
```

For the full validation schema reference, see [docs/API_VALIDATION.md](docs/API_VALIDATION.md).

## Testing

The test infrastructure uses Python and is organized into unit and integration tests:

```bash
# Run unit tests (once test files are added)
cd tests
python -m pytest unit/

# Run integration tests (requires the gateway to be running)
python -m pytest integration/
```

## Deployment

### Quick Start with install.sh

```bash
# Build and install
chmod +x install.sh
sudo ./install.sh
```

### NGINX Reverse Proxy (TLS Termination)

```nginx
upstream gateway {
    server 127.0.0.1:8080;
}

server {
    listen 443 ssl http2;
    server_name api.example.com;

    ssl_certificate     /etc/ssl/certs/api.example.com.pem;
    ssl_certificate_key /etc/ssl/private/api.example.com.key;

    location / {
        proxy_pass http://gateway;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        proxy_read_timeout 30s;
        proxy_send_timeout 30s;
    }
}
```

### Systemd Service

```ini
[Unit]
Description=Dynamic REST API Gateway
After=network.target

[Service]
Type=simple
ExecStart=/usr/local/bin/rest-api-gateway -c /etc/gateway/config.json
Restart=always
RestartSec=5
User=gateway
Group=gateway
Environment=GATEWAY_AES_KEY=<your-hex-key>
LimitNOFILE=65536

[Install]
WantedBy=multi-user.target
```

For complete deployment instructions, see [docs/DEPLOYMENT.md](docs/DEPLOYMENT.md).

## Project Structure

```
.
├── CMakeLists.txt
├── README.md
├── docs/
│   ├── ARCHITECTURE.md
│   ├── PROTOCOL.md
│   ├── CONFIGURATION.md
│   ├── API_VALIDATION.md
│   └── DEPLOYMENT.md
├── include/gateway/
│   ├── Common.h                  # Shared enums and type definitions
│   ├── core/
│   │   ├── Configuration.h       # Configuration management
│   │   ├── Coroutine.h           # Task<T> coroutine framework
│   │   └── EventLoop.h           # epoll-based event loop
│   ├── transport/
│   │   ├── TcpListener.h         # TCP socket listener
│   │   ├── HttpParser.h          # HTTP/1.1 request parser
│   │   ├── HttpResponse.h        # HTTP response builder
│   │   ├── UnixSocketListener.h  # Unix domain socket listener
│   │   └── FrameProtocol.h       # Binary frame protocol
│   ├── routing/
│   │   ├── EndpointDefinition.h  # Endpoint schema definition
│   │   ├── ParameterSchema.h     # Parameter validation schema
│   │   └── RoutingTable.h        # Dynamic routing table
│   ├── validation/
│   │   ├── JsonParser.h          # Hand-written JSON parser
│   │   └── ValidationEngine.h    # Request validation engine
│   ├── auth/
│   │   ├── AesGcm.h              # AES-256-GCM via AF_ALG
│   │   └── TokenEngine.h         # Token generation and validation
│   ├── forwarding/
│   │   ├── RequestFormatter.h    # Request serialization for backends
│   │   └── BackendForwarder.h    # Backend communication
│   └── logging/
│       └── Logger.h              # Structured JSON logging
├── src/                          # Implementation files (.cpp)
└── tests/
    ├── unit/                     # Unit tests
    └── integration/              # Integration tests
```

## License

This project is licensed under the MIT License.
