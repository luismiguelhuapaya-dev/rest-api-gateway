# Configuration Reference

The gateway accepts configuration from three sources. When a parameter is set in multiple sources, the highest-precedence source wins.

**Precedence order** (highest first):

1. Command-line arguments
2. JSON configuration file (loaded via `--config`)
3. Environment variables

## Configuration Parameters

### Network

| Parameter | CLI Flag | JSON Key | Env Variable | Type | Default | Description |
|---|---|---|---|---|---|---|
| Listen address | `-a`, `--address` | `listen_address` | `GATEWAY_ADDRESS` | string | `0.0.0.0` | IP address for the TCP listener to bind to. Use `127.0.0.1` for local-only access behind a reverse proxy. |
| Listen port | `-p`, `--port` | `listen_port` | `GATEWAY_PORT` | uint16 | `8080` | TCP port for incoming HTTP client connections. |
| Unix socket path | `-s`, `--socket` | `unix_socket_path` | `GATEWAY_SOCKET_PATH` | string | `/tmp/gateway.sock` | File system path for the Unix domain socket that backends connect to. |
| Max connections | `-m`, `--max-connections` | `max_connections` | -- | uint32 | `1024` | Maximum number of concurrent connections tracked by the event loop. Also controls the `epoll` event buffer size. |

### Timeouts and Limits

| Parameter | CLI Flag | JSON Key | Env Variable | Type | Default | Description |
|---|---|---|---|---|---|---|
| Read timeout | -- | `read_timeout_ms` | -- | uint32 | `30000` | Maximum time (milliseconds) to wait for data from a client or backend. |
| Write timeout | -- | `write_timeout_ms` | -- | uint32 | `30000` | Maximum time (milliseconds) to wait for a write to complete. |
| Max request body | -- | `max_request_body_size` | -- | uint32 | `1048576` | Maximum allowed HTTP request body size in bytes (1 MiB default). Requests exceeding this are rejected. |

### Authentication

| Parameter | CLI Flag | JSON Key | Env Variable | Type | Default | Description |
|---|---|---|---|---|---|---|
| AES key | -- | `aes_key` | `GATEWAY_AES_KEY` | string (hex) | *(none)* | 256-bit AES key as a 64-character hexadecimal string. Required for authentication features. When not set, the gateway starts in degraded mode and all `requires_auth` endpoints return 401. |
| Access token expiry | `--access-expiry` | `access_token_expiry_seconds` | -- | uint32 | `300` | Access token lifetime in seconds (5 minutes default). |
| Refresh token expiry | `--refresh-expiry` | `refresh_token_expiry_seconds` | -- | uint32 | `86400` | Refresh token lifetime in seconds (24 hours default). |

### Logging

| Parameter | CLI Flag | JSON Key | Env Variable | Type | Default | Description |
|---|---|---|---|---|---|---|
| Log level | `--log-level` | `log_level` | `GATEWAY_LOG_LEVEL` | string | `info` | Minimum log level. Messages below this level are discarded. Values: `debug`, `info`, `warning`, `error`, `fatal`. |
| Log file | `--log-file` | `log_file` | -- | string | `/var/log/gateway.log` | Path to the log output file. The file is opened in append mode. |
| Log to stdout | `--log-stdout` / `--no-log-stdout` | `log_to_stdout` | -- | bool | `true` | Whether to also write log messages to standard output. |

## Command-Line Reference

```
rest-api-gateway [options]

Options:
  -c, --config <file>          Load configuration from a JSON file.
                               File values are overridden by subsequent CLI flags.

  -p, --port <port>            TCP listen port.
                               Example: -p 9000

  -a, --address <addr>         TCP listen address.
                               Example: -a 127.0.0.1

  -s, --socket <path>          Unix domain socket path for backend connections.
                               Example: -s /run/gateway/backend.sock

  -m, --max-connections <n>    Maximum concurrent connections.
                               Example: -m 2048

  --log-level <level>          Set the minimum log level.
                               Values: debug, info, warning, error, fatal
                               Example: --log-level debug

  --log-file <path>            Log file path.
                               Example: --log-file /var/log/gateway/access.log

  --log-stdout                 Enable logging to standard output (default).

  --no-log-stdout              Disable logging to standard output.
                               Useful for production with log file rotation.

  --access-expiry <seconds>    Access token expiry in seconds.
                               Example: --access-expiry 600

  --refresh-expiry <seconds>   Refresh token expiry in seconds.
                               Example: --refresh-expiry 172800
```

## Environment Variables

Environment variables are loaded first, before the config file or CLI arguments. They are useful for injecting secrets (like the AES key) without writing them to disk.

| Variable | Description |
|---|---|
| `GATEWAY_AES_KEY` | AES-256 encryption key as a 64-character hex string. This is the recommended way to provide the encryption key, since config files may be committed to version control. |
| `GATEWAY_PORT` | TCP listen port. Overridden by `-p` / `--port`. |
| `GATEWAY_ADDRESS` | TCP listen address. Overridden by `-a` / `--address`. |
| `GATEWAY_SOCKET_PATH` | Unix socket path. Overridden by `-s` / `--socket`. |
| `GATEWAY_LOG_LEVEL` | Log level. Overridden by `--log-level`. |

### Generating an AES Key

```bash
# Using OpenSSL
export GATEWAY_AES_KEY="$(openssl rand -hex 32)"

# Using /dev/urandom
export GATEWAY_AES_KEY="$(head -c 32 /dev/urandom | xxd -p -c 64)"

# Static key for development (do NOT use in production)
export GATEWAY_AES_KEY="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
```

The key must be exactly 64 hex characters (representing 32 bytes / 256 bits). Invalid keys are silently rejected and authentication features remain disabled.

## JSON Configuration File

The configuration file is a JSON object with flat key-value pairs. All keys are optional -- unspecified keys use their default values.

### Complete Example

```json
{
    "listen_address": "127.0.0.1",
    "listen_port": 8080,
    "unix_socket_path": "/run/gateway/backend.sock",
    "max_connections": 2048,
    "read_timeout_ms": 15000,
    "write_timeout_ms": 15000,
    "max_request_body_size": 2097152,
    "access_token_expiry_seconds": 600,
    "refresh_token_expiry_seconds": 172800,
    "log_level": "info",
    "log_file": "/var/log/gateway/gateway.log",
    "log_to_stdout": false,
    "aes_key": "a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6c7d8e9f0a1b2"
}
```

### Minimal Example

```json
{
    "listen_port": 9000,
    "unix_socket_path": "/tmp/my-gateway.sock",
    "log_level": "debug"
}
```

### Loading

```bash
# Via CLI flag
./rest-api-gateway -c /etc/gateway/config.json

# The config file path can also be combined with other flags
# (CLI flags override config file values)
./rest-api-gateway -c config.json -p 9000 --log-level debug
```

## Validation Rules

- **Port:** Must be a valid 16-bit unsigned integer (0-65535). Ports below 1024 require root privileges.
- **Socket path:** The parent directory must exist and be writable. If the socket file already exists, the gateway will remove it before binding.
- **AES key:** Must be exactly 64 hexadecimal characters (`[0-9a-fA-F]`). Partial or malformed keys are rejected silently.
- **Log level:** Must be one of the five recognized values. Unrecognized values are ignored (the default `info` is used).
- **Timeouts:** Specified in milliseconds. A value of `0` means no timeout.
- **Max request body size:** In bytes. Default is 1 MiB. Increase for APIs that accept file uploads or large payloads.
- **Token expiry:** In seconds. The access token expiry should be short (minutes) and the refresh token expiry should be longer (hours to days).

## Recommended Production Configuration

```json
{
    "listen_address": "127.0.0.1",
    "listen_port": 8080,
    "unix_socket_path": "/run/gateway/backend.sock",
    "max_connections": 4096,
    "read_timeout_ms": 10000,
    "write_timeout_ms": 10000,
    "max_request_body_size": 1048576,
    "access_token_expiry_seconds": 300,
    "refresh_token_expiry_seconds": 86400,
    "log_level": "warning",
    "log_file": "/var/log/gateway/gateway.log",
    "log_to_stdout": false
}
```

Key considerations:

- Bind to `127.0.0.1` when running behind NGINX. Only bind to `0.0.0.0` if the gateway is directly exposed.
- Use `warning` or `error` log level in production to reduce I/O. Use `debug` only for troubleshooting.
- Disable stdout logging (`log_to_stdout: false`) and rely on the log file with log rotation.
- Set the AES key via the `GATEWAY_AES_KEY` environment variable, not in the config file.
- Place the Unix socket in `/run/gateway/` (a tmpfs mount) for performance and automatic cleanup on reboot.
