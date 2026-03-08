# Architecture

This document describes the internal architecture of the Dynamic REST API Gateway Server.

## System Context

```
                        +------------------+
                        |   HTTP Clients   |
                        | (browsers, apps, |
                        |  curl, etc.)     |
                        +--------+---------+
                                 |
                                 | HTTPS (port 443)
                                 |
                        +--------+---------+
                        |      NGINX       |
                        | (TLS termination,|
                        |  load balancing) |
                        +--------+---------+
                                 |
                                 | HTTP (port 8080)
                                 |
                  +--------------+--------------+
                  |     REST API Gateway        |
                  |                             |
                  |  +----------+  +----------+ |
                  |  |  TCP     |  | Unix     | |
                  |  | Listener |  | Socket   | |
                  |  |          |  | Listener | |
                  |  +----+-----+  +----+-----+ |
                  |       |             |        |
                  |  +----+-------------+-----+  |
                  |  |      Event Loop        |  |
                  |  | (epoll + coroutines)   |  |
                  |  +------------------------+  |
                  +--------------+--------------+
                                 |
                                 | Unix domain socket
                                 | (frame protocol)
                                 |
              +------------------+------------------+
              |                  |                  |
     +--------+------+  +-------+-------+  +-------+-------+
     | Backend A      |  | Backend B     |  | Backend C     |
     | (user-service) |  | (order-svc)   |  | (auth-svc)    |
     +----------------+  +---------------+  +---------------+
```

## Request Processing Pipeline

Every HTTP request passes through a six-stage pipeline. Each stage can short-circuit the pipeline by returning an error response.

### Stage 1: Parse

**Component:** `HttpParser`

The raw bytes read from the TCP socket are parsed into an `HttpRequest` structure:

```
Raw bytes --> HttpParser::Parse() --> HttpRequest {
    m_eMethod            (GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS)
    m_szPath             ("/api/users/abc123")
    m_szQueryString      ("page=1&limit=20")
    m_stdszszHeaders     ({"content-type": "application/json", ...})
    m_szBody             ("{\"name\": \"John\"}")
    m_stdszszQueryParameters  ({"page": "1", "limit": "20"})
}
```

Key implementation details:

- Maximum header size: 8192 bytes
- Maximum request line: 4096 bytes
- Supports chunked reading (incomplete requests are buffered and re-parsed)
- Query parameters are URL-decoded
- Header names are normalized to lowercase

### Stage 2: Route

**Component:** `RoutingTable`, `EndpointDefinition`

The parsed request is matched against the dynamically-registered routing table:

```
HttpRequest.path + HttpRequest.method
    --> RoutingTable::FindEndpoint()
    --> EndpointDefinition + extracted path parameters
```

Routing supports parameterized paths. The path `/api/users/{user_id}/orders/{order_id}` matches `/api/users/abc123/orders/456` and extracts `{user_id: "abc123", order_id: "456"}`.

The routing table is protected by a mutex, allowing backends to register or unregister endpoints concurrently without blocking request processing. Maximum capacity: 4096 endpoints.

### Stage 3: Authenticate

**Component:** `TokenEngine`, `AesGcm`

If the matched endpoint has `m_bRequiresAuthentication == true`, the gateway extracts the `Authorization: Bearer <token>` header and validates it:

```
Bearer token string
    --> Base64url decode
    --> AES-256-GCM decrypt (via AF_ALG kernel API)
    --> Deserialize TokenPayload
    --> Check: token type == Access
    --> Check: server_id == endpoint's backend_id
    --> Check: expiry > current time
    --> Check: not in revocation set
    --> Extract user_id, inject as x-authenticated-user header
```

On failure, the pipeline short-circuits with `401 Unauthorized`.

### Stage 4: Validate

**Component:** `ValidationEngine`, `ParameterSchema`

Every parameter declared in the endpoint's schema is validated:

| Location | Source | Validation |
|---|---|---|
| `path` | Extracted path parameters | Type, constraints |
| `query` | Parsed query string | Type, constraints, required check |
| `header` | Request headers | Type, constraints, required check |
| `body` | JSON-parsed request body | Type, structure, constraints, required check |

Validation is exhaustive -- all parameters are checked and all errors are collected before the response is sent:

```json
{
    "errors": [
        {"parameter": "age", "message": "Integer value -1 is below minimum 0", "provided_value": "-1"},
        {"parameter": "email", "message": "Required body field is missing"}
    ]
}
```

On failure, the pipeline short-circuits with `400 Bad Request`.

### Stage 5: Forward

**Component:** `BackendForwarder`, `RequestFormatter`, `FrameProtocol`

The validated request is serialized into a binary frame and sent to the appropriate backend:

```
HttpRequest + EndpointDefinition
    --> RequestFormatter::FormatRequestForBackend()
    --> Frame { type=Request, payload=JSON }
    --> FrameProtocol::SendFrame() (async write via coroutine)
    --> wait for response frame (async read via coroutine)
    --> FrameProtocol::ReceiveFrame()
    --> Frame { type=Response, payload=JSON }
```

The forwarder maintains per-backend read buffers protected by a mutex, ensuring that partial frame reads across multiple coroutine wakes are handled correctly.

### Stage 6: Respond

**Component:** `HttpResponse`

The backend's response frame is converted into an HTTP response:

```
Response Frame payload
    --> Parse JSON: extract status_code and body
    --> HttpResponse::SetStatusCode()
    --> HttpResponse::SetJsonBody()
    --> HttpResponse::Build() --> raw HTTP bytes
    --> async write to client TCP socket
    --> close connection
```

## Component Diagram

```
+------------------------------------------------------------------+
|                          GatewayServer                           |
|                                                                  |
|  +-----------------+    +------------------+    +--------------+ |
|  |  Configuration  |    |    EventLoop     |    |    Logger    | |
|  |                 |    |  (epoll-based)   |    | (singleton)  | |
|  | - JSON config   |    |  - fd management |    | - JSON fmt   | |
|  | - CLI args      |    |  - coroutine     |    | - file + tty | |
|  | - env vars      |    |    scheduling    |    |              | |
|  +-----------------+    +--------+---------+    +--------------+ |
|                                  |                               |
|          +----------+------------+------------+---------+        |
|          |          |                         |         |        |
|  +-------+---+ +---+----------+   +----------+--+ +----+-----+  |
|  |TcpListener| |UnixSocket    |   |BackendFwd   | |TokenEng  |  |
|  |           | |Listener      |   |             | |          |  |
|  |- bind()   | |- bind()      |   |- fwd req   | |- gen pair|  |
|  |- accept() | |- accept()    |   |- fwd login | |- validate|  |
|  |- read()   | |- register()  |   |- fwd refresh| |- refresh|  |
|  |- write()  | |- unregister()|   |             | |- revoke |  |
|  +-----+-----+ +------+------+   +------+------+ +----+-----+  |
|        |               |                |              |         |
|        |               |         +------+------+  +---+------+  |
|        |               |         |RequestFmt   |  |  AesGcm  |  |
|   +----+-----+    +----+----+    |- build JSON |  |- encrypt |  |
|   |HttpParser|    |FrameProt|    |- escape strs|  |- decrypt |  |
|   |- parse   |    |- ser    |    +-------------+  |- AF_ALG  |  |
|   |- url dec |    |- deser  |                     +----------+  |
|   +----------+    |- send   |                                    |
|                   |- recv   |    +------------------------------+ |
|   +----------+    +---------+    |       RoutingTable           | |
|   |HttpResp  |                   | - register/unregister        | |
|   |- build   |    +----------+   | - find endpoint (path match) | |
|   |- 200/400 |    |Validation|   | - mutex-protected            | |
|   |- 401/404 |    |Engine    |   +------------------------------+ |
|   |- 500/405 |    |- path    |                                    |
|   +----------+    |- query   |   +------------------------------+ |
|                   |- header  |   | EndpointDefinition           | |
|                   |- body    |   | - path with {params}         | |
|                   +----------+   | - method                     | |
|                                  | - parameter schemas          | |
|                                  | - requires_auth flag         | |
|                                  +------------------------------+ |
+------------------------------------------------------------------+
```

## Coroutine / Async I/O Model

The gateway uses a single-threaded, coroutine-based async I/O model built on Linux `epoll`:

### Task<T> Coroutine Framework

`Task<T>` is the core coroutine return type. It implements:

- **Lazy evaluation** -- coroutines are suspended at `initial_suspend()` and only run when scheduled.
- **Symmetric transfer** -- `final_suspend()` returns a `FinalAwaiter` that resumes the caller's continuation, avoiding stack overflow in deep coroutine chains.
- **Exception propagation** -- unhandled exceptions are captured in the promise and rethrown at `co_await` or `GetResult()`.

### EpollAwaiter

`EpollAwaiter` bridges coroutines with `epoll`. When a coroutine needs to wait for a file descriptor:

1. The coroutine calls `co_await eventLoop.WaitForReadable(fd)` (or `WaitForWritable`).
2. `EpollAwaiter::await_suspend()` registers the coroutine handle with `epoll_ctl(EPOLL_CTL_MOD)` using `EPOLLONESHOT`.
3. The coroutine is suspended, and control returns to the event loop.
4. When the fd becomes ready, `epoll_wait()` returns the coroutine handle, and the event loop resumes it.

### Event Loop Execution

```
EventLoop::Run()
    |
    +---> ProcessPendingCoroutines()   // Resume newly scheduled coroutines
    |         |
    |         +---> coroutine.resume() // Each coroutine runs until it hits co_await
    |
    +---> epoll_wait(100ms timeout)    // Wait for I/O events
    |         |
    |         +---> For each ready fd:
    |                   coroutine_handle::from_address(event.data.ptr)
    |                   handle.resume()
    |
    +---> Loop (while m_stdab8Running)
```

This model achieves high concurrency with a single thread and zero context-switch overhead.

## Memory Model

### Per-Connection State

Each client connection is represented by a `Task<void>` coroutine that owns:
- The client file descriptor (closed on coroutine exit)
- A stack-allocated `HttpParser`, `HttpRequest`, and `HttpResponse`
- No heap allocation for the common path (small request, cached endpoint lookup)

### Backend Connection State

Each backend connection maintains:
- A file descriptor mapped to a `BackendConnection` struct
- A per-backend read buffer (`std::string`) for partial frame reassembly
- An entry in the `RoutingTable` for each registered endpoint

### Shared State and Synchronization

| Resource | Protection | Contention |
|---|---|---|
| `RoutingTable::m_stdsEndpoints` | `std::mutex` | Low (register/unregister is rare) |
| `TokenEngine::m_stdszRevokedTokens` | `std::mutex` | Low (revocation is rare) |
| `BackendForwarder::m_stdsznReadBuffers` | `std::mutex` | Low (one buffer per backend) |
| `Logger` writes | `std::mutex` | Moderate (every request logs) |
| `RequestFormatter::m_un32NextRequestIdentifier` | `std::mutex` | Low (atomic increment) |

Because the event loop is single-threaded, most mutex contention occurs only when backends connect/disconnect or when tokens are revoked -- both infrequent operations.

## Error Handling Strategy

The gateway uses a result-code pattern throughout, avoiding exceptions for control flow:

1. **Every function returns a `bool`** indicating success or failure. Output parameters are populated only on success.
2. **Errors are logged immediately** at the point of failure using the structured `Logger`.
3. **Coroutine errors** are captured by the `Task<T>` promise's `unhandled_exception()` handler and propagated to the awaiting coroutine.
4. **Client-facing errors** are mapped to appropriate HTTP status codes:

| Internal Condition | HTTP Status | Response Body |
|---|---|---|
| Unknown path/method | 404 Not Found | `No endpoint registered for METHOD /path` |
| Missing/invalid token | 401 Unauthorized | `Valid authentication token required` |
| Schema validation failure | 400 Bad Request | `{"errors": [...]}` |
| Backend not connected | 500 Internal Server Error | `Backend not available: <id>` |
| Backend communication error | 500 Internal Server Error | `Failed to send request to backend` |
| Malformed HTTP request | 400 Bad Request | `Malformed HTTP request` |

5. **Backend disconnects** are detected by `FrameProtocol::ReceiveFrame()` returning a frame with `FrameType::Error`. The event loop handler removes all endpoints registered by that backend and cleans up the connection.

## Signal Handling

The gateway handles the following signals:

| Signal | Behavior |
|---|---|
| `SIGINT` | Graceful shutdown (stops the event loop) |
| `SIGTERM` | Graceful shutdown (stops the event loop) |
| `SIGPIPE` | Ignored (prevents crash on broken client connections) |
