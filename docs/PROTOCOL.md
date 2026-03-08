# Protocol Specification

This document defines the binary frame protocol used for communication between the gateway and backend services over Unix domain sockets.

## Transport Layer

Communication between the gateway and backend services uses **Unix domain sockets** (`AF_UNIX`, `SOCK_STREAM`). The default socket path is `/tmp/gateway.sock`, configurable via the `unix_socket_path` configuration parameter.

Backends connect as clients to the gateway's listening Unix socket. The connection is persistent -- a backend stays connected for the duration of its lifecycle.

## Frame Format

All communication is framed. Each frame consists of a fixed 12-byte header followed by a variable-length payload.

### Header Layout (12 bytes)

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         Frame Type                            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        Request ID                             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Payload Length                           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
|                     Payload (JSON)                            |
|                       (N bytes)                               |
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

| Offset | Size | Field | Encoding |
|--------|------|-------|----------|
| 0 | 4 bytes | Frame Type | `uint32`, big-endian |
| 4 | 4 bytes | Request ID | `uint32`, big-endian |
| 8 | 4 bytes | Payload Length | `uint32`, big-endian |
| 12 | N bytes | Payload | UTF-8 JSON string |

**Maximum payload size:** 1,048,576 bytes (1 MiB)

### Byte Ordering

All multi-byte integer fields use **network byte order** (big-endian). The serialization logic:

```
Write uint32 to bytes:
    byte[0] = (value >> 24) & 0xFF
    byte[1] = (value >> 16) & 0xFF
    byte[2] = (value >>  8) & 0xFF
    byte[3] = (value >>  0) & 0xFF

Read uint32 from bytes:
    value = (byte[0] << 24) | (byte[1] << 16) | (byte[2] << 8) | byte[3]
```

## Frame Types

| Value | Name | Direction | Description |
|-------|------|-----------|-------------|
| `0` | `Registration` | Backend --> Gateway | Backend registers its identity and endpoints |
| `1` | `Unregistration` | Backend --> Gateway | Backend announces graceful disconnect |
| `2` | `Request` | Gateway --> Backend | Forwarded HTTP client request |
| `3` | `Response` | Backend --> Gateway | Backend's response to a request |
| `4` | `LoginResponse` | Backend --> Gateway | Authentication result from backend |
| `5` | `TokenResponse` | Gateway --> Backend | Issued token pair sent to backend |
| `6` | `Heartbeat` | Bidirectional | Keep-alive ping/pong |
| `7` | `Error` | Either | Error condition or connection lost |

## Protocol Sequences

### 1. Registration Protocol

The first frame a backend sends after connecting must be a Registration frame.

```
Backend                                    Gateway
   |                                         |
   |--- [Registration Frame] --------------->|
   |    type=0, payload={                    |
   |      "backend_id": "my-service",       |
   |      "endpoints": [...]                 |
   |    }                                    |
   |                                         |
   |    Gateway registers backend and        |
   |    adds endpoints to routing table      |
   |                                         |
   |<-- [Response Frame] -------------------|
   |    type=3, payload={                    |
   |      "status": "registered",           |
   |      "backend_id": "my-service"        |
   |    }                                    |
   |                                         |
```

#### Registration Payload Schema

```json
{
    "backend_id": "<string: unique identifier for this backend>",
    "endpoints": [
        {
            "path": "<string: URL path pattern, e.g. /api/users/{id}>",
            "method": "<string: HTTP method, e.g. GET, POST, PUT, DELETE>",
            "description": "<string: optional human-readable description>",
            "backend": "<string: optional, overridden by backend_id>",
            "requires_auth": "<boolean: whether this endpoint requires authentication>",
            "parameters": [
                {
                    "name": "<string: parameter name>",
                    "type": "<string: string|integer|float|boolean|object|array>",
                    "location": "<string: path|query|header|body>",
                    "required": "<boolean: whether this parameter is required>",
                    "description": "<string: optional description>",
                    "default": "<string: optional default value>",
                    "constraints": {
                        "min_value": "<integer: minimum numeric value>",
                        "max_value": "<integer: maximum numeric value>",
                        "min_length": "<integer: minimum string length>",
                        "max_length": "<integer: maximum string length>",
                        "pattern": "<string: regex pattern for string validation>",
                        "allowed_values": ["<string>", "<string>", "..."]
                    }
                }
            ]
        }
    ]
}
```

#### Registration Acknowledgment

```json
{
    "status": "registered",
    "backend_id": "<string: the registered backend ID>"
}
```

### 2. Request/Response Protocol

Once registered, the gateway forwards matching HTTP requests to the backend.

```
Client          Gateway                          Backend
  |                |                                |
  |-- HTTP req --->|                                |
  |                |--- [Request Frame] ----------->|
  |                |    type=2, id=42, payload={    |
  |                |      "method": "GET",          |
  |                |      "path": "/api/users/123", |
  |                |      "backend": "user-svc",    |
  |                |      ...                       |
  |                |    }                           |
  |                |                                |
  |                |<-- [Response Frame] -----------|
  |                |    type=3, id=42, payload={    |
  |                |      "status_code": 200,       |
  |                |      "body": {...}             |
  |                |    }                           |
  |                |                                |
  |<- HTTP resp ---|                                |
```

#### Request Payload Schema

```json
{
    "method": "<string: HTTP method>",
    "path": "<string: original request path>",
    "backend": "<string: backend identifier>",
    "path_parameters": {
        "<name>": "<value>",
        "...": "..."
    },
    "query_parameters": {
        "<name>": "<value>",
        "...": "..."
    },
    "headers": {
        "content-type": "<value>",
        "accept": "<value>",
        "user-agent": "<value>",
        "x-request-id": "<value>",
        "x-forwarded-for": "<value>"
    },
    "body": "<object|string: request body, JSON object if parseable>"
}
```

**Note:** Only selected headers are forwarded: `content-type`, `accept`, `user-agent`, `x-request-id`, and `x-forwarded-for`. The authenticated user ID (if available) is injected as `x-authenticated-user` into the headers map.

#### Response Payload Schema

```json
{
    "status_code": "<integer: HTTP status code to return to client>",
    "body": "<object|string: response body>"
}
```

If `status_code` is omitted, the gateway defaults to `200`. If the entire payload is not valid JSON, it is returned as-is with status `200`.

### 3. Login Protocol (Two-Frame Exchange)

Authentication follows a two-frame exchange between the gateway and backend:

```
Client          Gateway                          Backend
  |                |                                |
  |-- POST login ->|                                |
  |                |--- [Request Frame] ----------->|
  |                |    type=2, id=99, payload={    |
  |                |      "method": "POST",         |
  |                |      "path": "/auth/login",    |
  |                |      "body": {                 |
  |                |        "username": "alice",     |
  |                |        "password": "s3cret"     |
  |                |      }                         |
  |                |    }                           |
  |                |                                |
  |                |<-- [LoginResponse Frame] ------|
  |                |    type=4, id=99, payload={    |
  |                |      "success": true,          |
  |                |      "user_id": "user_42",     |
  |                |      "server_id": "auth-svc"   |
  |                |    }                           |
  |                |                                |
  |                |    Gateway generates token     |
  |                |    pair via AES-256-GCM        |
  |                |                                |
  |                |--- [TokenResponse Frame] ----->|
  |                |    type=5, id=99, payload={    |
  |                |      "access_token": "...",    |
  |                |      "refresh_token": "...",   |
  |                |      "user_id": "user_42"      |
  |                |    }                           |
  |                |                                |
  |<- HTTP 200 ----|                                |
  |  {                                              |
  |    "access_token": "...",                       |
  |    "refresh_token": "...",                      |
  |    "token_type": "Bearer",                     |
  |    "expires_in": ...                            |
  |  }                                              |
```

#### LoginResponse Payload Schema

Sent by the backend after verifying credentials:

```json
{
    "success": "<boolean: true if credentials are valid>",
    "user_id": "<string: unique user identifier (required if success=true)>",
    "server_id": "<string: optional, defaults to backend_id>",
    "message": "<string: optional, error message if success=false>"
}
```

#### TokenResponse Payload Schema

Sent by the gateway to the backend after generating tokens:

```json
{
    "access_token": "<string: base64url-encoded encrypted access token>",
    "refresh_token": "<string: base64url-encoded encrypted refresh token>",
    "user_id": "<string: the authenticated user's identifier>"
}
```

### 4. Token Refresh Protocol

Token refresh is handled entirely by the gateway without backend involvement:

```
Client                   Gateway
  |                        |
  |-- POST /auth/refresh ->|
  |   {                    |
  |     "refresh_token":   |
  |       "..."            |
  |   }                    |
  |                        |
  |   Gateway decrypts the |
  |   refresh token,       |
  |   validates it,        |
  |   revokes the old one, |
  |   generates a new pair |
  |                        |
  |<-- HTTP 200 -----------|
  |   {                    |
  |     "access_token":    |
  |       "...",           |
  |     "refresh_token":   |
  |       "...",           |
  |     "token_type":      |
  |       "Bearer"         |
  |   }                    |
```

### 5. Unregistration Protocol

Backends send an Unregistration frame to disconnect gracefully:

```
Backend                                    Gateway
   |                                         |
   |--- [Unregistration Frame] ------------->|
   |    type=1, payload={                    |
   |      "backend_id": "my-service"         |
   |    }                                    |
   |                                         |
   |    Gateway removes all endpoints for    |
   |    this backend_id and cleans up        |
   |                                         |
   |    (connection closed)                  |
```

#### Unregistration Payload Schema

```json
{
    "backend_id": "<string: identifier of the backend disconnecting>"
}
```

### 6. Heartbeat Protocol

Either side can send a Heartbeat frame. The receiver responds with a Heartbeat frame:

```
Backend                                    Gateway
   |                                         |
   |--- [Heartbeat Frame] ----------------->|
   |    type=6, payload={}                  |
   |                                         |
   |<-- [Heartbeat Frame] ------------------|
   |    type=6, payload={                    |
   |      "status": "alive"                  |
   |    }                                    |
```

### 7. Error Handling

If the gateway detects a backend disconnect (read returns 0 or an unrecoverable error):

1. A frame with `FrameType::Error` is synthesized internally.
2. All endpoints registered by the disconnected backend are removed from the routing table.
3. The backend's file descriptor and connection state are cleaned up.
4. The event is logged.

If a client sends a request to a backend that has disconnected between routing and forwarding, the gateway returns `500 Internal Server Error` with `"Backend not available: <backend_id>"`.

## Token Binary Format

Tokens are not transmitted in the frame protocol directly -- they are carried as JSON string fields within LoginResponse and TokenResponse payloads. The binary format below describes the plaintext structure before AES-256-GCM encryption.

### Token Plaintext Layout

```
Offset   Size        Field
------   --------    -----
0        4 bytes     Server ID length (L1, uint32 big-endian)
4        L1 bytes    Server ID (UTF-8 string)
4+L1     4 bytes     User ID length (L2, uint32 big-endian)
8+L1     L2 bytes    User ID (UTF-8 string)
8+L1+L2  1 byte      Token type (0x00 = Access, 0x01 = Refresh)
9+L1+L2  8 bytes     Creation timestamp (int64, big-endian, Unix epoch seconds)
17+L1+L2 8 bytes     Expiry timestamp (int64, big-endian, Unix epoch seconds)
```

Minimum plaintext size (empty server and user IDs): 25 bytes.

### Token Wire Format

```
[12 bytes: Random IV] [encrypted plaintext] [16 bytes: GCM authentication tag]
```

The entire byte sequence is then base64url-encoded (no padding) to produce the token string used in HTTP `Authorization: Bearer` headers.

### Encryption Details

- **Algorithm:** AES-256-GCM
- **Key size:** 256 bits (32 bytes)
- **IV size:** 96 bits (12 bytes), randomly generated per token via `getrandom()`
- **Tag size:** 128 bits (16 bytes)
- **Additional Authenticated Data (AAD):** None (empty)
- **Implementation:** Linux kernel Crypto API via `AF_ALG` socket (`aead`, `gcm(aes)`)

## Implementation Notes

### Partial Frame Handling

The protocol implementation handles partial reads correctly. The `FrameProtocol::ReceiveFrame()` coroutine:

1. Checks if the read buffer already contains a complete frame (`HasCompleteFrame()`).
2. If not, suspends via `co_await WaitForReadable(fd)` and reads more data.
3. Appends new data to the buffer and rechecks.
4. Once a complete frame is available, it is deserialized and the consumed bytes are erased from the buffer.
5. Remaining bytes stay in the buffer for the next frame.

### Request ID Correlation

Each request frame carries a monotonically increasing `uint32` request ID generated by the `RequestFormatter`. Response frames echo back the same request ID. This allows the gateway to correlate responses with requests, though the current implementation processes requests sequentially per backend connection.

### Backend Multiplexing

Multiple HTTP clients can be served by the same backend concurrently. The gateway maintains a per-backend read buffer in `BackendForwarder::m_stdsznReadBuffers` (mutex-protected) to handle interleaved frame data from partial reads.
