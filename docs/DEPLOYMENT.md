# Deployment Guide

This guide covers building, installing, and running the Dynamic REST API Gateway in a production environment.

## Prerequisites

### System Requirements

| Component | Requirement |
|---|---|
| Operating system | Linux (kernel 5.x or newer) |
| C++ compiler | GCC 12+ or Clang 14+ with C++20 support |
| Build system | CMake 3.20+ |
| Kernel modules | `algif_aead` (for AES-256-GCM via AF_ALG) |

### Verifying Kernel Support

The gateway requires the Linux kernel crypto API for AES-256-GCM encryption. Verify that the necessary kernel module is available:

```bash
# Check if AF_ALG is supported
modprobe algif_aead
lsmod | grep algif_aead

# Verify gcm(aes) is available
cat /proc/crypto | grep -A5 "gcm(aes)"
```

If `algif_aead` is not loaded, add it to `/etc/modules-load.d/`:

```bash
echo "algif_aead" | sudo tee /etc/modules-load.d/gateway.conf
sudo modprobe algif_aead
```

## Building from Source

### Standard Build

```bash
git clone <repository-url>
cd rest-api-gateway

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

The compiled binary is `build/rest-api-gateway`.

### Build Options

```bash
# Debug build (with symbols, no optimization)
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Release build (optimized, no debug symbols)
cmake .. -DCMAKE_BUILD_TYPE=Release

# Specify compiler
cmake .. -DCMAKE_CXX_COMPILER=g++-12

# Generate compile_commands.json for IDE integration
cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

### Verifying the Build

```bash
# Check the binary
file build/rest-api-gateway
# Expected: ELF 64-bit LSB executable, x86-64, dynamically linked

# Test that it starts (will fail without a proper environment, but verifies the binary)
./build/rest-api-gateway --help 2>&1 || true
```

## Installation

### Manual Installation

```bash
# Install the binary
sudo install -m 755 build/rest-api-gateway /usr/local/bin/

# Create configuration directory
sudo mkdir -p /etc/gateway
sudo cp config.json /etc/gateway/config.json
sudo chmod 600 /etc/gateway/config.json

# Create log directory
sudo mkdir -p /var/log/gateway
sudo chown gateway:gateway /var/log/gateway

# Create runtime directory for the Unix socket
sudo mkdir -p /run/gateway
sudo chown gateway:gateway /run/gateway

# Create a dedicated service user
sudo useradd -r -s /usr/sbin/nologin -d /nonexistent gateway
```

### Using install.sh

If an `install.sh` script is provided:

```bash
chmod +x install.sh
sudo ./install.sh
```

This typically handles building, installing the binary, creating directories, and setting up the systemd service.

## NGINX Reverse Proxy Configuration

In production, the gateway should sit behind NGINX for TLS termination, rate limiting, and static file serving.

### Basic Configuration

```nginx
upstream gateway_backend {
    server 127.0.0.1:8080;
    keepalive 32;
}

server {
    listen 443 ssl http2;
    server_name api.example.com;

    # TLS configuration
    ssl_certificate     /etc/ssl/certs/api.example.com.pem;
    ssl_certificate_key /etc/ssl/private/api.example.com.key;
    ssl_protocols       TLSv1.2 TLSv1.3;
    ssl_ciphers         HIGH:!aNULL:!MD5;
    ssl_prefer_server_ciphers on;
    ssl_session_cache   shared:SSL:10m;
    ssl_session_timeout 10m;

    # Security headers
    add_header Strict-Transport-Security "max-age=31536000; includeSubDomains" always;
    add_header X-Content-Type-Options "nosniff" always;
    add_header X-Frame-Options "DENY" always;

    # Client body size limit (should match gateway's max_request_body_size)
    client_max_body_size 1m;

    # Proxy settings
    location / {
        proxy_pass http://gateway_backend;

        proxy_http_version 1.1;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        proxy_set_header Connection "";

        proxy_connect_timeout 5s;
        proxy_read_timeout 30s;
        proxy_send_timeout 30s;

        proxy_buffering off;
    }

    # Health check endpoint (optional - requires a backend to register /health)
    location /health {
        proxy_pass http://gateway_backend;
        access_log off;
    }
}

# HTTP to HTTPS redirect
server {
    listen 80;
    server_name api.example.com;
    return 301 https://$server_name$request_uri;
}
```

### Rate Limiting

```nginx
# Define rate limit zones (in http block)
limit_req_zone $binary_remote_addr zone=api:10m rate=100r/s;
limit_req_zone $binary_remote_addr zone=auth:10m rate=10r/m;

server {
    # ... SSL config ...

    # Rate limit all API requests
    location /api/ {
        limit_req zone=api burst=50 nodelay;
        proxy_pass http://gateway_backend;
        # ... proxy settings ...
    }

    # Stricter rate limit for authentication endpoints
    location /auth/ {
        limit_req zone=auth burst=5 nodelay;
        proxy_pass http://gateway_backend;
        # ... proxy settings ...
    }
}
```

## Systemd Service

### Service File

Create `/etc/systemd/system/rest-api-gateway.service`:

```ini
[Unit]
Description=Dynamic REST API Gateway
Documentation=https://github.com/your-org/rest-api-gateway
After=network.target
Wants=network-online.target

[Service]
Type=simple
User=gateway
Group=gateway

# Binary and configuration
ExecStart=/usr/local/bin/rest-api-gateway -c /etc/gateway/config.json
WorkingDirectory=/var/lib/gateway

# Environment (AES key should be in an environment file)
EnvironmentFile=-/etc/gateway/env

# Restart policy
Restart=always
RestartSec=5
StartLimitBurst=5
StartLimitIntervalSec=60

# Resource limits
LimitNOFILE=65536
LimitNPROC=4096

# Security hardening
NoNewPrivileges=true
ProtectSystem=strict
ProtectHome=true
PrivateTmp=true
PrivateDevices=true
ProtectKernelTunables=true
ProtectKernelModules=true
ProtectControlGroups=true
RestrictSUIDSGID=true
RestrictNamespaces=true

# Allow AF_ALG sockets and Unix domain sockets
RestrictAddressFamilies=AF_UNIX AF_INET AF_INET6 AF_ALG

# Read-write paths
ReadWritePaths=/var/log/gateway /run/gateway

# Standard output/error
StandardOutput=journal
StandardError=journal
SyslogIdentifier=rest-api-gateway

[Install]
WantedBy=multi-user.target
```

### Environment File

Create `/etc/gateway/env` with restricted permissions:

```bash
# /etc/gateway/env
GATEWAY_AES_KEY=your-64-character-hex-key-here
```

```bash
sudo chmod 600 /etc/gateway/env
sudo chown gateway:gateway /etc/gateway/env
```

### Managing the Service

```bash
# Reload systemd after creating/modifying the service file
sudo systemctl daemon-reload

# Enable the service to start on boot
sudo systemctl enable rest-api-gateway

# Start the service
sudo systemctl start rest-api-gateway

# Check status
sudo systemctl status rest-api-gateway

# View logs
sudo journalctl -u rest-api-gateway -f

# Restart
sudo systemctl restart rest-api-gateway

# Stop
sudo systemctl stop rest-api-gateway
```

## Runtime Directory Setup

The Unix socket needs a directory that persists across reboots but is cleaned on each boot. Use `tmpfiles.d`:

Create `/etc/tmpfiles.d/gateway.conf`:

```
d /run/gateway 0755 gateway gateway -
```

This creates `/run/gateway` on every boot, owned by the `gateway` user.

## Security Considerations

### AES Key Management

- **Never** store the AES key in the JSON config file in production. Use the `GATEWAY_AES_KEY` environment variable or a systemd `EnvironmentFile`.
- Rotate the AES key periodically. When the key changes, all existing tokens become invalid (this is by design -- no migration needed).
- Use a cryptographically secure random number generator: `openssl rand -hex 32`.

### File Permissions

```bash
# Configuration (contains no secrets if AES key is in env)
sudo chmod 644 /etc/gateway/config.json

# Environment file (contains AES key)
sudo chmod 600 /etc/gateway/env
sudo chown gateway:gateway /etc/gateway/env

# Binary
sudo chmod 755 /usr/local/bin/rest-api-gateway

# Log directory
sudo chmod 750 /var/log/gateway
sudo chown gateway:gateway /var/log/gateway

# Socket directory
sudo chmod 755 /run/gateway
sudo chown gateway:gateway /run/gateway
```

### Network Isolation

- Bind the gateway to `127.0.0.1` (not `0.0.0.0`) when running behind NGINX.
- Use firewall rules to block direct access to the gateway port (8080) from external networks.
- The Unix domain socket provides inherent network isolation for backend communication.

### Systemd Hardening

The systemd service file above includes comprehensive security directives:

- `NoNewPrivileges=true` -- prevents privilege escalation
- `ProtectSystem=strict` -- mounts `/` read-only except for explicitly allowed paths
- `PrivateTmp=true` -- uses a private `/tmp` namespace
- `RestrictAddressFamilies` -- limits socket types to only what is needed
- `ProtectKernelTunables=true` -- prevents writing to `/proc` and `/sys`

### Unix Socket Permissions

Control which processes can connect as backends by setting permissions on the socket directory:

```bash
# Only members of the 'gateway' group can connect
sudo chmod 750 /run/gateway
sudo chown gateway:gateway /run/gateway

# Add backend service users to the gateway group
sudo usermod -aG gateway backend-user
```

## Performance Tuning

### File Descriptor Limits

The gateway needs file descriptors for: the epoll instance, the TCP listener, the Unix socket listener, each client connection, and each backend connection. Set the limit high enough:

```bash
# In the systemd service
LimitNOFILE=65536

# Or system-wide in /etc/security/limits.conf
gateway  soft  nofile  65536
gateway  hard  nofile  65536
```

### Socket Backlog

The default TCP backlog is 128. For high-traffic deployments, increase the kernel limit:

```bash
# Temporary
sudo sysctl -w net.core.somaxconn=4096

# Permanent (in /etc/sysctl.d/gateway.conf)
net.core.somaxconn = 4096
```

### Epoll Event Buffer

Set `max_connections` in the gateway config to match expected concurrency:

```json
{
    "max_connections": 4096
}
```

This controls the size of the `epoll_event` array allocated for `epoll_wait()`.

### Connection Timeouts

Reduce timeouts to free up resources from stale connections:

```json
{
    "read_timeout_ms": 10000,
    "write_timeout_ms": 10000
}
```

### NGINX Connection Pooling

Enable keepalive connections between NGINX and the gateway to avoid TCP handshake overhead:

```nginx
upstream gateway_backend {
    server 127.0.0.1:8080;
    keepalive 32;
}
```

## Monitoring and Logging

### Log Format

The gateway outputs structured JSON logs. Each log entry includes:

```json
{
    "timestamp": "2025-01-15T10:30:00.123Z",
    "level": "info",
    "component": "Main",
    "message": "Gateway running on 127.0.0.1:8080"
}
```

Request logs include additional fields:

```json
{
    "timestamp": "2025-01-15T10:30:01.456Z",
    "level": "info",
    "component": "Request",
    "method": "POST",
    "path": "/api/users",
    "status_code": 201,
    "duration_ms": 12.34
}
```

### Log Rotation

Use `logrotate` to manage log files:

Create `/etc/logrotate.d/gateway`:

```
/var/log/gateway/*.log {
    daily
    rotate 30
    compress
    delaycompress
    missingok
    notifempty
    copytruncate
    su gateway gateway
}
```

The `copytruncate` directive is important because the gateway holds the log file open. This avoids the need for a signal-based reload.

### Health Monitoring

Since the gateway has no built-in health endpoint (all endpoints are dynamic), you have two options:

1. **Register a health endpoint** from a backend:

```json
{
    "backend_id": "health-checker",
    "endpoints": [
        {
            "path": "/health",
            "method": "GET",
            "requires_auth": false
        }
    ]
}
```

2. **Use a TCP health check** in your load balancer or monitoring system:

```bash
# Check if the gateway is accepting TCP connections
nc -z 127.0.0.1 8080 && echo "UP" || echo "DOWN"
```

### Monitoring with systemd

```bash
# Watch logs in real time
sudo journalctl -u rest-api-gateway -f

# Show logs since last boot
sudo journalctl -u rest-api-gateway -b

# Show logs from the last hour
sudo journalctl -u rest-api-gateway --since "1 hour ago"

# Check if the service is running
systemctl is-active rest-api-gateway
```

## Troubleshooting

### Gateway fails to start

| Symptom | Cause | Solution |
|---|---|---|
| `Failed to create epoll file descriptor` | Insufficient permissions or kernel support | Check `LimitNOFILE` and kernel version |
| `Failed to bind TCP socket` | Port already in use or insufficient permissions | Check with `ss -tlnp` and use a port > 1024 or run as root |
| `Failed to bind Unix socket` | Socket file exists or directory permissions | Remove stale socket file or check directory ownership |
| `Failed to create AF_ALG socket` | Kernel module not loaded | Run `modprobe algif_aead` |
| `No AES key configured` | Warning, not an error. Auth features disabled. | Set `GATEWAY_AES_KEY` environment variable |

### Backend cannot connect

| Symptom | Cause | Solution |
|---|---|---|
| Connection refused | Gateway not running or wrong socket path | Verify the Unix socket path matches |
| Permission denied | Socket directory permissions | Add the backend user to the `gateway` group |
| Socket not found | Gateway crashed or socket path mismatch | Check gateway status and socket path config |

### Authentication failures

| Symptom | Cause | Solution |
|---|---|---|
| All auth requests return 401 | No AES key configured | Set `GATEWAY_AES_KEY` |
| Token suddenly invalid | AES key rotated | Expected behavior -- clients must re-authenticate |
| Token expired | Access token lifetime exceeded | Use the refresh token at `/auth/refresh` |
| Refresh token rejected | Refresh token already used (rotation) | Re-authenticate with credentials |

### Logging

Enable debug logging to diagnose issues:

```bash
./rest-api-gateway -c config.json --log-level debug --log-stdout
```

Debug-level logging includes detailed information about frame I/O, token operations, and routing decisions.
