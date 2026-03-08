#!/usr/bin/env bash
###############################################################################
# install.sh — Dynamic REST API Gateway Server
#
# Zero-touch installer for Ubuntu/Debian.
#
# Usage:
#   sudo ./install.sh <TARGET_DIR>
#
# Example:
#   sudo ./install.sh /opt/rest-api-gateway
#
# What this does (fully automated, no prompts):
#   1. Installs build prerequisites via apt (gcc-12, cmake, etc.)
#   2. Validates compiler/tool versions
#   3. Creates a 'gateway' service user
#   4. Builds the application from source
#   5. Installs binary, config, docs, systemd service, logrotate
#   6. Generates a random AES-256 key (if one doesn't already exist)
#   7. Starts and enables the service
#
# Idempotent — safe to re-run. Preserves existing config and AES key.
# Target OS: Ubuntu 20.04+ / Debian 11+
###############################################################################

set -euo pipefail
export DEBIAN_FRONTEND=noninteractive

# ── Colors ──────────────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[0;33m'
BLUE='\033[0;34m'; CYAN='\033[0;36m'; BOLD='\033[1m'; NC='\033[0m'

ok()   { echo -e "  ${GREEN}✓${NC} $1"; }
warn() { echo -e "  ${YELLOW}!${NC} $1"; }
err()  { echo -e "  ${RED}✗${NC} $1"; }
info() { echo -e "  ${BLUE}→${NC} $1"; }
step() { echo -e "\n${BOLD}${CYAN}[$1]${NC} $2"; }

die() { err "$1"; exit 1; }

# ── Argument handling ───────────────────────────────────────────────────────
if [[ $# -lt 1 ]] || [[ "$1" == "--help" ]] || [[ "$1" == "-h" ]]; then
    echo "Usage: sudo $0 <TARGET_DIR>"
    echo ""
    echo "  TARGET_DIR   Where to install (e.g. /opt/rest-api-gateway)"
    echo ""
    echo "Example:"
    echo "  sudo $0 /opt/rest-api-gateway"
    exit 0
fi

TARGET_DIR="$1"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SERVICE_USER="gateway"
SERVICE_GROUP="gateway"
BUILD_DIR=""

VERSION="1.0.0"
[[ -f "${SCRIPT_DIR}/packaging/VERSION" ]] && VERSION="$(tr -d '[:space:]' < "${SCRIPT_DIR}/packaging/VERSION")"
[[ -f "${SCRIPT_DIR}/VERSION" ]] && VERSION="$(tr -d '[:space:]' < "${SCRIPT_DIR}/VERSION")"

# ── Cleanup on failure ──────────────────────────────────────────────────────
cleanup() {
    local rc=$?
    if [[ ${rc} -ne 0 ]]; then
        echo ""
        err "Installation failed (exit code ${rc}). See output above."
        [[ -n "${BUILD_DIR}" ]] && [[ -d "${BUILD_DIR}" ]] && info "Build dir preserved: ${BUILD_DIR}"
    fi
}
trap cleanup EXIT

# ── Banner ──────────────────────────────────────────────────────────────────
echo ""
echo -e "${BOLD}${BLUE}═══════════════════════════════════════════════════════════${NC}"
echo -e "${BOLD}${BLUE}  REST API Gateway Server — Installer v${VERSION}${NC}"
echo -e "${BOLD}${BLUE}═══════════════════════════════════════════════════════════${NC}"
echo -e "  Target: ${TARGET_DIR}"

# ── 1. Pre-flight checks ───────────────────────────────────────────────────
step "1/9" "Pre-flight checks"

[[ "$(uname -s)" == "Linux" ]] || die "This installer only runs on Linux."
ok "Running on Linux $(uname -r)"

[[ "$(id -u)" -eq 0 ]] || die "Must run as root. Use: sudo $0 ${TARGET_DIR}"
ok "Running as root"

# Verify Ubuntu/Debian
if [[ -f /etc/os-release ]]; then
    . /etc/os-release
    case "${ID}" in
        ubuntu|debian) ok "Detected ${PRETTY_NAME}" ;;
        *) die "Unsupported distribution: ${ID}. This installer supports Ubuntu/Debian only." ;;
    esac
else
    die "Cannot detect distribution (/etc/os-release not found)."
fi

# Locate source tree
if [[ -f "${SCRIPT_DIR}/CMakeLists.txt" ]]; then
    SRC_ROOT="${SCRIPT_DIR}"
elif [[ -f "${SCRIPT_DIR}/../CMakeLists.txt" ]]; then
    SRC_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
else
    die "Cannot find CMakeLists.txt. Run this script from the extracted tar directory."
fi
ok "Source tree: ${SRC_ROOT}"

# ── 2. Install prerequisites ───────────────────────────────────────────────
step "2/9" "Installing build prerequisites (apt)"

apt-get update -qq 2>&1 | tail -1
apt-get install -y -qq \
    build-essential \
    g++-12 \
    gcc-12 \
    cmake \
    make \
    ninja-build \
    pkg-config \
    2>&1 | tail -1

# Point default gcc/g++ to version 12+ if the default is older
GCC_MAJOR="$(g++ -dumpversion 2>/dev/null | cut -d. -f1)" || GCC_MAJOR="0"
if [[ "${GCC_MAJOR}" -lt 12 ]]; then
    # g++-12 was just installed; set up alternatives
    if command -v g++-12 &>/dev/null; then
        update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-12 120 2>/dev/null || true
        update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-12 120 2>/dev/null || true
        update-alternatives --set g++ /usr/bin/g++-12 2>/dev/null || true
        update-alternatives --set gcc /usr/bin/gcc-12 2>/dev/null || true
        info "Set g++-12 as default compiler"
    fi
fi

ok "Build prerequisites installed"

# ── 3. Validate tools ──────────────────────────────────────────────────────
step "3/9" "Validating compiler and build tools"

# Find best C++ compiler
CXX=""
for candidate in g++-12 g++-13 g++-14 g++; do
    if command -v "${candidate}" &>/dev/null; then
        ver="$("${candidate}" -dumpversion | cut -d. -f1)"
        if [[ "${ver}" -ge 12 ]]; then
            CXX="${candidate}"
            ok "${candidate} version $("${candidate}" -dumpversion) (>= 12 required)"
            break
        fi
    fi
done
[[ -n "${CXX}" ]] || die "No GCC 12+ found. Install with: apt install g++-12"

# CMake
command -v cmake &>/dev/null || die "CMake not found."
CMAKE_VER="$(cmake --version | head -1 | grep -oP '\d+\.\d+')"
CMAKE_MAJ="$(echo "${CMAKE_VER}" | cut -d. -f1)"
CMAKE_MIN="$(echo "${CMAKE_VER}" | cut -d. -f2)"
if [[ "${CMAKE_MAJ}" -lt 3 ]] || { [[ "${CMAKE_MAJ}" -eq 3 ]] && [[ "${CMAKE_MIN}" -lt 20 ]]; }; then
    die "CMake ${CMAKE_VER} found but 3.20+ required."
fi
ok "CMake ${CMAKE_VER}"

# Build tool
if command -v ninja &>/dev/null; then
    BUILD_GEN="Ninja"
    ok "Build tool: Ninja"
else
    BUILD_GEN="Unix Makefiles"
    ok "Build tool: Make"
fi

# ── 4. Create service user ─────────────────────────────────────────────────
step "4/9" "Service user"

if id "${SERVICE_USER}" &>/dev/null; then
    ok "User '${SERVICE_USER}' already exists"
else
    useradd --system --no-create-home --shell /usr/sbin/nologin --comment "REST API Gateway" "${SERVICE_USER}"
    ok "Created system user '${SERVICE_USER}'"
fi

# ── 5. Create directory structure ──────────────────────────────────────────
step "5/9" "Creating directory structure at ${TARGET_DIR}"

for d in bin etc logs run share/doc; do
    mkdir -p "${TARGET_DIR}/${d}"
done
ok "Directories created"

# ── 6. Build from source ──────────────────────────────────────────────────
step "6/9" "Building from source"

BUILD_DIR="$(mktemp -d /tmp/gw-build.XXXXXX)"
info "Build dir: ${BUILD_DIR}"
info "Compiler: ${CXX}"

cmake \
    -S "${SRC_ROOT}" \
    -B "${BUILD_DIR}" \
    -G "${BUILD_GEN}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER="${CXX}" \
    -DCMAKE_VERBOSE_MAKEFILE=OFF \
    > /dev/null 2>&1

cmake --build "${BUILD_DIR}" --config Release -j"$(nproc)" > /dev/null 2>&1

# Find the binary (could be named differently depending on CMakeLists.txt)
BINARY=""
for name in rest-api-gateway gateway rest_api_gateway; do
    [[ -f "${BUILD_DIR}/${name}" ]] && BINARY="${BUILD_DIR}/${name}" && break
done
[[ -n "${BINARY}" ]] || die "Build succeeded but binary not found in ${BUILD_DIR}"

install -m 0755 "${BINARY}" "${TARGET_DIR}/bin/rest-api-gateway"
ok "Binary installed: ${TARGET_DIR}/bin/rest-api-gateway"

rm -rf "${BUILD_DIR}"
BUILD_DIR=""

# ── 7. Install config, service, logrotate ──────────────────────────────────
step "7/9" "Installing configuration and service files"

# --- Config file (don't overwrite existing) ---
if [[ ! -f "${TARGET_DIR}/etc/gateway.json" ]]; then
    cat > "${TARGET_DIR}/etc/gateway.json" <<'CFGEOF'
{
    "listen_address": "0.0.0.0",
    "listen_port": 8080,
    "unix_socket_path": "__TARGET__/run/gateway.sock",
    "max_connections": 10000,
    "max_header_size": 8192,
    "max_body_size": 1048576,
    "access_token_expiry": 300,
    "refresh_token_expiry": 86400,
    "aes_key": "",
    "log_level": "info",
    "backend_timeout": 30000
}
CFGEOF
    sed -i "s|__TARGET__|${TARGET_DIR}|g" "${TARGET_DIR}/etc/gateway.json"
    ok "Configuration: ${TARGET_DIR}/etc/gateway.json"
else
    warn "Config already exists — preserved"
fi

# --- AES-256 key ---
ENV_FILE="${TARGET_DIR}/etc/gateway.env"
if [[ -f "${ENV_FILE}" ]] && grep -q "GATEWAY_AES_KEY=.\+" "${ENV_FILE}" 2>/dev/null; then
    ok "AES-256 key already exists — preserved"
else
    AES_KEY="$(head -c 32 /dev/urandom | xxd -p -c 64)"
    echo "GATEWAY_AES_KEY=${AES_KEY}" > "${ENV_FILE}"
    ok "Generated AES-256 key: ${ENV_FILE}"
fi

# --- systemd service ---
cat > /etc/systemd/system/rest-api-gateway.service <<SVCEOF
[Unit]
Description=Dynamic REST API Gateway Server
After=network.target
Wants=network-online.target

[Service]
Type=simple
User=${SERVICE_USER}
Group=${SERVICE_GROUP}
ExecStart=${TARGET_DIR}/bin/rest-api-gateway --config ${TARGET_DIR}/etc/gateway.json
WorkingDirectory=${TARGET_DIR}
Restart=always
RestartSec=5
NoNewPrivileges=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=${TARGET_DIR}/logs ${TARGET_DIR}/run
PrivateTmp=true
LimitNOFILE=65536
LimitNPROC=4096
EnvironmentFile=-${TARGET_DIR}/etc/gateway.env

[Install]
WantedBy=multi-user.target
SVCEOF
systemctl daemon-reload
ok "systemd service: rest-api-gateway.service"

# --- logrotate ---
cat > /etc/logrotate.d/rest-api-gateway <<LREOF
${TARGET_DIR}/logs/*.log {
    daily
    missingok
    rotate 14
    compress
    delaycompress
    notifempty
    create 0640 ${SERVICE_USER} ${SERVICE_GROUP}
    sharedscripts
    postrotate
        systemctl kill --signal=HUP rest-api-gateway 2>/dev/null || true
    endscript
}
LREOF
ok "logrotate: /etc/logrotate.d/rest-api-gateway"

# --- NGINX example (just copy for reference) ---
for src in "${SRC_ROOT}/packaging/nginx-gateway.conf" "${SCRIPT_DIR}/packaging/nginx-gateway.conf"; do
    if [[ -f "${src}" ]]; then
        cp "${src}" "${TARGET_DIR}/share/doc/nginx-gateway.conf.example"
        ok "NGINX example: ${TARGET_DIR}/share/doc/nginx-gateway.conf.example"
        break
    fi
done

# --- Documentation ---
[[ -f "${SRC_ROOT}/README.md" ]] && cp "${SRC_ROOT}/README.md" "${TARGET_DIR}/share/doc/"
[[ -d "${SRC_ROOT}/docs" ]] && cp -r "${SRC_ROOT}/docs/." "${TARGET_DIR}/share/doc/"
echo "${VERSION}" > "${TARGET_DIR}/share/doc/VERSION"
ok "Documentation installed"

# ── 8. Set permissions ─────────────────────────────────────────────────────
step "8/9" "Setting permissions"

chown -R root:${SERVICE_GROUP} "${TARGET_DIR}"
chown -R ${SERVICE_USER}:${SERVICE_GROUP} "${TARGET_DIR}/logs" "${TARGET_DIR}/run"
chmod 0755 "${TARGET_DIR}/bin/rest-api-gateway"
chmod 0640 "${TARGET_DIR}/etc/gateway.json"
chmod 0600 "${TARGET_DIR}/etc/gateway.env"
chmod 0750 "${TARGET_DIR}/logs" "${TARGET_DIR}/run"
chown root:${SERVICE_GROUP} "${TARGET_DIR}/etc/gateway.json" "${TARGET_DIR}/etc/gateway.env"
ok "Permissions set"

# ── 9. Start the service ──────────────────────────────────────────────────
step "9/9" "Starting service"

systemctl enable rest-api-gateway --quiet 2>/dev/null
systemctl restart rest-api-gateway
sleep 1

if systemctl is-active --quiet rest-api-gateway; then
    ok "Service is running"
else
    warn "Service may still be starting — check: systemctl status rest-api-gateway"
fi

# ── Done ────────────────────────────────────────────────────────────────────
echo ""
echo -e "${BOLD}${GREEN}═══════════════════════════════════════════════════════════${NC}"
echo -e "${BOLD}${GREEN}  Installation complete!${NC}"
echo -e "${BOLD}${GREEN}═══════════════════════════════════════════════════════════${NC}"
echo ""
echo -e "  Binary:  ${TARGET_DIR}/bin/rest-api-gateway"
echo -e "  Config:  ${TARGET_DIR}/etc/gateway.json"
echo -e "  Logs:    ${TARGET_DIR}/logs/"
echo -e "  Socket:  ${TARGET_DIR}/run/gateway.sock"
echo ""
echo -e "  ${CYAN}Status:${NC}   systemctl status rest-api-gateway"
echo -e "  ${CYAN}Logs:${NC}     journalctl -u rest-api-gateway -f"
echo -e "  ${CYAN}Restart:${NC}  systemctl restart rest-api-gateway"
echo ""
