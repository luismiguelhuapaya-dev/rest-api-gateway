#!/usr/bin/env bash
###############################################################################
# install.sh - Dynamic REST API Gateway Server Installer
#
# Usage: ./install.sh [TARGET_DIR]
#   TARGET_DIR defaults to /opt/rest-api-gateway
#
# Supported distributions:
#   - Ubuntu 20.04+, Debian 11+
#   - CentOS/RHEL 8+
#   - Fedora 36+
#
# This script is idempotent and can be safely re-run.
###############################################################################

set -euo pipefail

# ─── Color codes and formatting ──────────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# ─── Status output helpers ───────────────────────────────────────────────────
step_count=0

print_header() {
    echo ""
    echo -e "${BOLD}${BLUE}================================================================${NC}"
    echo -e "${BOLD}${BLUE}  Dynamic REST API Gateway Server - Installer${NC}"
    echo -e "${BOLD}${BLUE}================================================================${NC}"
    echo ""
}

print_step() {
    step_count=$((step_count + 1))
    echo -e "${BOLD}${CYAN}[Step ${step_count}]${NC} $1"
}

print_ok() {
    echo -e "  ${GREEN}[OK]${NC} $1"
}

print_warn() {
    echo -e "  ${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "  ${RED}[ERROR]${NC} $1"
}

print_info() {
    echo -e "  ${BLUE}[INFO]${NC} $1"
}

# ─── Error handler ───────────────────────────────────────────────────────────
cleanup() {
    local exit_code=$?
    if [[ ${exit_code} -ne 0 ]]; then
        echo ""
        print_error "Installation failed at step ${step_count} (exit code: ${exit_code})"
        print_info "Check the output above for details."
        if [[ -n "${BUILD_DIR:-}" ]] && [[ -d "${BUILD_DIR}" ]]; then
            print_info "Build directory preserved for debugging: ${BUILD_DIR}"
        fi
    fi
}
trap cleanup EXIT

# ─── Configuration ───────────────────────────────────────────────────────────
TARGET_DIR="${1:-/opt/rest-api-gateway}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
SERVICE_USER="gateway"
SERVICE_GROUP="gateway"
BUILD_DIR=""
VERSION="unknown"

if [[ -f "${SCRIPT_DIR}/VERSION" ]]; then
    VERSION="$(cat "${SCRIPT_DIR}/VERSION" | tr -d '[:space:]')"
elif [[ -f "${SOURCE_DIR}/packaging/VERSION" ]]; then
    VERSION="$(cat "${SOURCE_DIR}/packaging/VERSION" | tr -d '[:space:]')"
fi

# ─── Pre-flight checks ──────────────────────────────────────────────────────
print_header

print_step "Running pre-flight checks"

# Must be running on Linux
if [[ "$(uname -s)" != "Linux" ]]; then
    print_error "This installer only supports Linux systems."
    print_info "Detected OS: $(uname -s)"
    exit 1
fi
print_ok "Running on Linux ($(uname -r))"

# Must be root
if [[ "$(id -u)" -ne 0 ]]; then
    print_error "This installer must be run as root (or with sudo)."
    exit 1
fi
print_ok "Running as root"

# ─── Detect Linux distribution ──────────────────────────────────────────────
print_step "Detecting Linux distribution"

DISTRO="unknown"
DISTRO_VERSION=""
PKG_MANAGER=""

if [[ -f /etc/os-release ]]; then
    # shellcheck disable=SC1091
    source /etc/os-release
    DISTRO="${ID}"
    DISTRO_VERSION="${VERSION_ID:-}"
fi

case "${DISTRO}" in
    ubuntu|debian)
        PKG_MANAGER="apt"
        print_ok "Detected ${PRETTY_NAME:-${DISTRO} ${DISTRO_VERSION}}"
        ;;
    centos|rhel|rocky|almalinux)
        if command -v dnf &>/dev/null; then
            PKG_MANAGER="dnf"
        else
            PKG_MANAGER="yum"
        fi
        print_ok "Detected ${PRETTY_NAME:-${DISTRO} ${DISTRO_VERSION}}"
        ;;
    fedora)
        PKG_MANAGER="dnf"
        print_ok "Detected ${PRETTY_NAME:-${DISTRO} ${DISTRO_VERSION}}"
        ;;
    *)
        print_warn "Unrecognized distribution: ${DISTRO}"
        # Try to detect package manager
        if command -v apt-get &>/dev/null; then
            PKG_MANAGER="apt"
            print_info "Using apt package manager"
        elif command -v dnf &>/dev/null; then
            PKG_MANAGER="dnf"
            print_info "Using dnf package manager"
        elif command -v yum &>/dev/null; then
            PKG_MANAGER="yum"
            print_info "Using yum package manager"
        else
            print_error "No supported package manager found (apt, dnf, or yum required)."
            exit 1
        fi
        ;;
esac

# ─── Install prerequisites ──────────────────────────────────────────────────
print_step "Checking and installing build prerequisites"

install_packages_apt() {
    print_info "Updating apt package lists..."
    apt-get update -qq

    local packages=(
        build-essential
        gcc
        g++
        cmake
        make
        ninja-build
        linux-headers-"$(uname -r)"
        pkg-config
    )

    print_info "Installing packages: ${packages[*]}"
    apt-get install -y -qq "${packages[@]}"
}

install_packages_dnf_yum() {
    local mgr="$1"

    print_info "Installing Development Tools group..."
    ${mgr} groupinstall -y "Development Tools" 2>/dev/null || true

    local packages=(
        gcc
        gcc-c++
        cmake
        make
        ninja-build
        kernel-headers
        kernel-devel
        pkgconfig
    )

    print_info "Installing packages: ${packages[*]}"
    ${mgr} install -y "${packages[@]}"
}

case "${PKG_MANAGER}" in
    apt)    install_packages_apt ;;
    dnf)    install_packages_dnf_yum dnf ;;
    yum)    install_packages_dnf_yum yum ;;
esac

# ─── Validate compiler versions ─────────────────────────────────────────────
print_step "Validating compiler and tool versions"

# Check for a suitable C++ compiler
CXX_COMPILER=""
CXX_VERSION=""

if command -v g++ &>/dev/null; then
    GCC_VERSION_FULL="$(g++ -dumpversion)"
    GCC_MAJOR="$(echo "${GCC_VERSION_FULL}" | cut -d. -f1)"
    if [[ "${GCC_MAJOR}" -ge 12 ]]; then
        CXX_COMPILER="g++"
        CXX_VERSION="${GCC_VERSION_FULL}"
        print_ok "GCC ${GCC_VERSION_FULL} (>= 12 required)"
    else
        print_warn "GCC ${GCC_VERSION_FULL} found but version 12+ is required for C++20 coroutines"
    fi
fi

if [[ -z "${CXX_COMPILER}" ]] && command -v clang++ &>/dev/null; then
    CLANG_VERSION_FULL="$(clang++ --version | head -1 | grep -oP '\d+\.\d+\.\d+' | head -1)"
    CLANG_MAJOR="$(echo "${CLANG_VERSION_FULL}" | cut -d. -f1)"
    if [[ "${CLANG_MAJOR}" -ge 14 ]]; then
        CXX_COMPILER="clang++"
        CXX_VERSION="${CLANG_VERSION_FULL}"
        print_ok "Clang ${CLANG_VERSION_FULL} (>= 14 required)"
    else
        print_warn "Clang ${CLANG_VERSION_FULL} found but version 14+ is required"
    fi
fi

if [[ -z "${CXX_COMPILER}" ]]; then
    print_error "No suitable C++ compiler found. Need GCC 12+ or Clang 14+."
    print_info "On Ubuntu/Debian: apt install g++-12"
    print_info "On Fedora/RHEL:   dnf install gcc-toolset-12-gcc-c++"
    exit 1
fi

# Check CMake version
if ! command -v cmake &>/dev/null; then
    print_error "CMake is not installed."
    exit 1
fi

CMAKE_VERSION="$(cmake --version | head -1 | grep -oP '\d+\.\d+(\.\d+)?')"
CMAKE_MAJOR="$(echo "${CMAKE_VERSION}" | cut -d. -f1)"
CMAKE_MINOR="$(echo "${CMAKE_VERSION}" | cut -d. -f2)"

if [[ "${CMAKE_MAJOR}" -lt 3 ]] || { [[ "${CMAKE_MAJOR}" -eq 3 ]] && [[ "${CMAKE_MINOR}" -lt 20 ]]; }; then
    print_error "CMake ${CMAKE_VERSION} found but version 3.20+ is required."
    exit 1
fi
print_ok "CMake ${CMAKE_VERSION} (>= 3.20 required)"

# Check for make or ninja
BUILD_TOOL=""
CMAKE_GENERATOR=""
if command -v ninja &>/dev/null; then
    BUILD_TOOL="ninja"
    CMAKE_GENERATOR="Ninja"
    print_ok "Ninja build tool found"
elif command -v make &>/dev/null; then
    BUILD_TOOL="make"
    CMAKE_GENERATOR="Unix Makefiles"
    print_ok "Make build tool found"
else
    print_error "Neither make nor ninja found."
    exit 1
fi

# ─── Create service user ────────────────────────────────────────────────────
print_step "Creating service user '${SERVICE_USER}'"

if id "${SERVICE_USER}" &>/dev/null; then
    print_ok "User '${SERVICE_USER}' already exists"
else
    useradd --system --no-create-home --shell /usr/sbin/nologin --comment "REST API Gateway" "${SERVICE_USER}"
    print_ok "Created system user '${SERVICE_USER}'"
fi

# ─── Create directory structure ──────────────────────────────────────────────
print_step "Creating directory structure at ${TARGET_DIR}"

declare -a DIRS=(
    "${TARGET_DIR}/bin"
    "${TARGET_DIR}/etc"
    "${TARGET_DIR}/lib"
    "${TARGET_DIR}/logs"
    "${TARGET_DIR}/run"
    "${TARGET_DIR}/share/doc"
)

for dir in "${DIRS[@]}"; do
    mkdir -p "${dir}"
    print_ok "Created ${dir}"
done

# ─── Build from source ──────────────────────────────────────────────────────
print_step "Building application from source"

BUILD_DIR="$(mktemp -d /tmp/rest-api-gateway-build.XXXXXX)"
print_info "Build directory: ${BUILD_DIR}"

# Determine source location (could be extracted tarball or repo)
if [[ -f "${SOURCE_DIR}/CMakeLists.txt" ]]; then
    SRC_ROOT="${SOURCE_DIR}"
else
    print_error "Cannot find CMakeLists.txt. Expected at: ${SOURCE_DIR}/CMakeLists.txt"
    exit 1
fi

print_info "Source root: ${SRC_ROOT}"
print_info "Compiler: ${CXX_COMPILER} ${CXX_VERSION}"
print_info "Build system: ${BUILD_TOOL} (${CMAKE_GENERATOR})"

# Configure
print_info "Running CMake configure..."
cmake \
    -S "${SRC_ROOT}" \
    -B "${BUILD_DIR}" \
    -G "${CMAKE_GENERATOR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER="${CXX_COMPILER}" \
    2>&1 | while IFS= read -r line; do echo "    ${line}"; done

# Build
print_info "Compiling (this may take a few minutes)..."
cmake --build "${BUILD_DIR}" --config Release -j"$(nproc)" \
    2>&1 | while IFS= read -r line; do echo "    ${line}"; done

# Verify binary was produced
if [[ ! -f "${BUILD_DIR}/rest-api-gateway" ]]; then
    print_error "Build completed but binary not found at ${BUILD_DIR}/rest-api-gateway"
    exit 1
fi
print_ok "Build successful"

# ─── Install binary ─────────────────────────────────────────────────────────
print_step "Installing binary"

install -m 0755 "${BUILD_DIR}/rest-api-gateway" "${TARGET_DIR}/bin/rest-api-gateway"
print_ok "Installed ${TARGET_DIR}/bin/rest-api-gateway"

# Clean up build directory
rm -rf "${BUILD_DIR}"
BUILD_DIR=""
print_ok "Cleaned up build directory"

# ─── Install configuration ──────────────────────────────────────────────────
print_step "Installing configuration files"

# Install default config (do not overwrite existing)
TEMPLATE_FILE=""
if [[ -f "${SCRIPT_DIR}/gateway.json.template" ]]; then
    TEMPLATE_FILE="${SCRIPT_DIR}/gateway.json.template"
elif [[ -f "${SOURCE_DIR}/packaging/gateway.json.template" ]]; then
    TEMPLATE_FILE="${SOURCE_DIR}/packaging/gateway.json.template"
fi

if [[ -f "${TARGET_DIR}/etc/gateway.json" ]]; then
    print_warn "Configuration file already exists, not overwriting: ${TARGET_DIR}/etc/gateway.json"
    if [[ -n "${TEMPLATE_FILE}" ]]; then
        cp "${TEMPLATE_FILE}" "${TARGET_DIR}/etc/gateway.json.new"
        print_info "New template saved as: ${TARGET_DIR}/etc/gateway.json.new"
    fi
else
    if [[ -n "${TEMPLATE_FILE}" ]]; then
        # Strip _comment fields from template to produce clean config
        grep -v '"_comment' "${TEMPLATE_FILE}" > "${TARGET_DIR}/etc/gateway.json"
        # Update unix_socket_path to match target directory
        sed -i "s|/var/run/rest-api-gateway/gateway.sock|${TARGET_DIR}/run/gateway.sock|g" "${TARGET_DIR}/etc/gateway.json"
        print_ok "Installed default configuration: ${TARGET_DIR}/etc/gateway.json"
    else
        # Create minimal config inline
        cat > "${TARGET_DIR}/etc/gateway.json" <<CONFIGEOF
{
    "listen_address": "0.0.0.0",
    "listen_port": 8080,
    "unix_socket_path": "${TARGET_DIR}/run/gateway.sock",
    "max_connections": 10000,
    "max_header_size": 8192,
    "max_body_size": 1048576,
    "access_token_expiry": 300,
    "refresh_token_expiry": 86400,
    "aes_key": "",
    "log_level": "info",
    "backend_timeout": 30000
}
CONFIGEOF
        print_ok "Created default configuration: ${TARGET_DIR}/etc/gateway.json"
    fi
fi

# Install configuration template for reference
if [[ -n "${TEMPLATE_FILE}" ]]; then
    cp "${TEMPLATE_FILE}" "${TARGET_DIR}/etc/gateway.json.template"
    print_ok "Installed configuration template: ${TARGET_DIR}/etc/gateway.json.template"
fi

# ─── Generate AES-256 key ───────────────────────────────────────────────────
print_step "Checking AES-256 encryption key"

ENV_FILE="${TARGET_DIR}/etc/gateway.env"

if [[ -f "${ENV_FILE}" ]] && grep -q "GATEWAY_AES_KEY=" "${ENV_FILE}" 2>/dev/null; then
    EXISTING_KEY="$(grep 'GATEWAY_AES_KEY=' "${ENV_FILE}" | cut -d= -f2)"
    if [[ -n "${EXISTING_KEY}" ]]; then
        print_ok "AES-256 key already exists in ${ENV_FILE}"
    else
        AES_KEY="$(head -c 32 /dev/urandom | xxd -p -c 64)"
        sed -i "s|^GATEWAY_AES_KEY=.*|GATEWAY_AES_KEY=${AES_KEY}|" "${ENV_FILE}"
        print_ok "Generated new AES-256 key in ${ENV_FILE}"
    fi
else
    AES_KEY="$(head -c 32 /dev/urandom | xxd -p -c 64)"
    echo "GATEWAY_AES_KEY=${AES_KEY}" > "${ENV_FILE}"
    print_ok "Generated AES-256 key: ${ENV_FILE}"
fi

# ─── Install systemd service ────────────────────────────────────────────────
print_step "Installing systemd service"

SERVICE_SOURCE=""
if [[ -f "${SCRIPT_DIR}/rest-api-gateway.service" ]]; then
    SERVICE_SOURCE="${SCRIPT_DIR}/rest-api-gateway.service"
elif [[ -f "${SOURCE_DIR}/packaging/rest-api-gateway.service" ]]; then
    SERVICE_SOURCE="${SOURCE_DIR}/packaging/rest-api-gateway.service"
fi

if [[ -n "${SERVICE_SOURCE}" ]]; then
    # Copy and customize the service file for the target directory
    sed \
        -e "s|/opt/rest-api-gateway|${TARGET_DIR}|g" \
        "${SERVICE_SOURCE}" > "${TARGET_DIR}/etc/rest-api-gateway.service"
    print_ok "Installed service file: ${TARGET_DIR}/etc/rest-api-gateway.service"
else
    # Generate service file inline
    cat > "${TARGET_DIR}/etc/rest-api-gateway.service" <<SERVICEEOF
[Unit]
Description=Dynamic REST API Gateway Server
After=network.target
Wants=network-online.target

[Service]
Type=simple
User=${SERVICE_USER}
Group=${SERVICE_GROUP}
ExecStart=${TARGET_DIR}/bin/rest-api-gateway --config ${TARGET_DIR}/etc/gateway.json --log-file ${TARGET_DIR}/logs/gateway.log --no-log-stdout
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
Environment=GATEWAY_AES_KEY=
EnvironmentFile=-${TARGET_DIR}/etc/gateway.env
RuntimeDirectory=rest-api-gateway
RuntimeDirectoryMode=0750

[Install]
WantedBy=multi-user.target
SERVICEEOF
    print_ok "Generated service file: ${TARGET_DIR}/etc/rest-api-gateway.service"
fi

# Symlink into systemd directory if systemd is available
if command -v systemctl &>/dev/null; then
    ln -sf "${TARGET_DIR}/etc/rest-api-gateway.service" /etc/systemd/system/rest-api-gateway.service
    systemctl daemon-reload
    print_ok "Registered systemd service"
else
    print_warn "systemd not found; service file installed but not registered"
fi

# ─── Install logrotate configuration ────────────────────────────────────────
print_step "Installing logrotate configuration"

LOGROTATE_SOURCE=""
if [[ -f "${SCRIPT_DIR}/rest-api-gateway.logrotate" ]]; then
    LOGROTATE_SOURCE="${SCRIPT_DIR}/rest-api-gateway.logrotate"
elif [[ -f "${SOURCE_DIR}/packaging/rest-api-gateway.logrotate" ]]; then
    LOGROTATE_SOURCE="${SOURCE_DIR}/packaging/rest-api-gateway.logrotate"
fi

if [[ -n "${LOGROTATE_SOURCE}" ]]; then
    sed \
        -e "s|/opt/rest-api-gateway|${TARGET_DIR}|g" \
        "${LOGROTATE_SOURCE}" > "${TARGET_DIR}/etc/rest-api-gateway.logrotate"
else
    cat > "${TARGET_DIR}/etc/rest-api-gateway.logrotate" <<LOGROTATEEOF
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
        if [ -f ${TARGET_DIR}/run/gateway.pid ]; then
            kill -HUP \$(cat ${TARGET_DIR}/run/gateway.pid) 2>/dev/null || true
        fi
    endscript
}
LOGROTATEEOF
fi
print_ok "Installed logrotate config: ${TARGET_DIR}/etc/rest-api-gateway.logrotate"

# Symlink into logrotate directory if it exists
if [[ -d /etc/logrotate.d ]]; then
    ln -sf "${TARGET_DIR}/etc/rest-api-gateway.logrotate" /etc/logrotate.d/rest-api-gateway
    print_ok "Registered logrotate configuration"
else
    print_warn "logrotate directory not found; config installed but not registered"
fi

# ─── Install documentation ──────────────────────────────────────────────────
print_step "Installing documentation"

# Copy README if it exists
if [[ -f "${SOURCE_DIR}/README.md" ]]; then
    cp "${SOURCE_DIR}/README.md" "${TARGET_DIR}/share/doc/README.md"
    print_ok "Installed README.md"
fi

# Copy docs directory if it exists
if [[ -d "${SOURCE_DIR}/docs" ]]; then
    cp -r "${SOURCE_DIR}/docs/." "${TARGET_DIR}/share/doc/"
    print_ok "Installed documentation from docs/"
fi

# Copy coding standards
if [[ -d "${SOURCE_DIR}/Coding Standards" ]]; then
    cp -r "${SOURCE_DIR}/Coding Standards/." "${TARGET_DIR}/share/doc/"
    print_ok "Installed coding standards"
fi

# Copy NGINX example config
NGINX_SOURCE=""
if [[ -f "${SCRIPT_DIR}/nginx-gateway.conf" ]]; then
    NGINX_SOURCE="${SCRIPT_DIR}/nginx-gateway.conf"
elif [[ -f "${SOURCE_DIR}/packaging/nginx-gateway.conf" ]]; then
    NGINX_SOURCE="${SOURCE_DIR}/packaging/nginx-gateway.conf"
fi

if [[ -n "${NGINX_SOURCE}" ]]; then
    cp "${NGINX_SOURCE}" "${TARGET_DIR}/share/doc/nginx-gateway.conf.example"
    print_ok "Installed NGINX example config: ${TARGET_DIR}/share/doc/nginx-gateway.conf.example"
fi

# Write version file
echo "${VERSION}" > "${TARGET_DIR}/share/doc/VERSION"
print_ok "Version: ${VERSION}"

# ─── Set file permissions ───────────────────────────────────────────────────
print_step "Setting file permissions"

# Binary: executable by owner and group
chmod 0755 "${TARGET_DIR}/bin/rest-api-gateway"
print_ok "Binary: 0755"

# Config files: readable by service user only
chmod 0640 "${TARGET_DIR}/etc/gateway.json"
chmod 0600 "${TARGET_DIR}/etc/gateway.env"
print_ok "Config: 0640 (gateway.json), 0600 (gateway.env)"

# Directories
chown -R root:${SERVICE_GROUP} "${TARGET_DIR}"
chown -R ${SERVICE_USER}:${SERVICE_GROUP} "${TARGET_DIR}/logs"
chown -R ${SERVICE_USER}:${SERVICE_GROUP} "${TARGET_DIR}/run"
chmod 0750 "${TARGET_DIR}/logs"
chmod 0750 "${TARGET_DIR}/run"
print_ok "Logs and run directories owned by ${SERVICE_USER}:${SERVICE_GROUP}"

# Ensure the service user can read config
chown root:${SERVICE_GROUP} "${TARGET_DIR}/etc/gateway.json"
chown root:${SERVICE_GROUP} "${TARGET_DIR}/etc/gateway.env"
print_ok "Configuration files accessible by ${SERVICE_GROUP} group"

# ─── Validate installation ──────────────────────────────────────────────────
print_step "Validating installation"

VALIDATION_PASSED=true

# Check binary exists and is executable
if [[ -x "${TARGET_DIR}/bin/rest-api-gateway" ]]; then
    print_ok "Binary is installed and executable"
else
    print_error "Binary not found or not executable"
    VALIDATION_PASSED=false
fi

# Check configuration exists
if [[ -f "${TARGET_DIR}/etc/gateway.json" ]]; then
    print_ok "Configuration file exists"
else
    print_error "Configuration file not found"
    VALIDATION_PASSED=false
fi

# Check AES key exists
if [[ -f "${TARGET_DIR}/etc/gateway.env" ]] && grep -q "GATEWAY_AES_KEY=" "${TARGET_DIR}/etc/gateway.env"; then
    print_ok "AES-256 key is configured"
else
    print_warn "AES-256 key not found in environment file"
fi

# Check service user exists
if id "${SERVICE_USER}" &>/dev/null; then
    print_ok "Service user '${SERVICE_USER}' exists"
else
    print_error "Service user '${SERVICE_USER}' does not exist"
    VALIDATION_PASSED=false
fi

# Check systemd service
if [[ -f /etc/systemd/system/rest-api-gateway.service ]]; then
    print_ok "Systemd service registered"
else
    print_warn "Systemd service not registered"
fi

# Check directory structure
for dir in bin etc lib logs run share/doc; do
    if [[ -d "${TARGET_DIR}/${dir}" ]]; then
        print_ok "Directory exists: ${TARGET_DIR}/${dir}"
    else
        print_error "Missing directory: ${TARGET_DIR}/${dir}"
        VALIDATION_PASSED=false
    fi
done

if [[ "${VALIDATION_PASSED}" != "true" ]]; then
    print_error "Installation validation failed. See errors above."
    exit 1
fi

# ─── Print post-installation instructions ───────────────────────────────────
echo ""
echo -e "${BOLD}${GREEN}================================================================${NC}"
echo -e "${BOLD}${GREEN}  Installation Complete!${NC}"
echo -e "${BOLD}${GREEN}================================================================${NC}"
echo ""
echo -e "${BOLD}Installation Summary:${NC}"
echo -e "  Version:        ${VERSION}"
echo -e "  Install path:   ${TARGET_DIR}"
echo -e "  Binary:         ${TARGET_DIR}/bin/rest-api-gateway"
echo -e "  Configuration:  ${TARGET_DIR}/etc/gateway.json"
echo -e "  Logs:           ${TARGET_DIR}/logs/"
echo -e "  Service user:   ${SERVICE_USER}"
echo ""
echo -e "${BOLD}Getting Started:${NC}"
echo ""
echo -e "  ${CYAN}1. Review and edit the configuration:${NC}"
echo -e "     vi ${TARGET_DIR}/etc/gateway.json"
echo ""
echo -e "  ${CYAN}2. Start the service:${NC}"
echo -e "     systemctl start rest-api-gateway"
echo ""
echo -e "  ${CYAN}3. Enable the service at boot:${NC}"
echo -e "     systemctl enable rest-api-gateway"
echo ""
echo -e "  ${CYAN}4. Check service status:${NC}"
echo -e "     systemctl status rest-api-gateway"
echo ""
echo -e "  ${CYAN}5. View logs:${NC}"
echo -e "     journalctl -u rest-api-gateway -f"
echo -e "     tail -f ${TARGET_DIR}/logs/gateway.log"
echo ""
echo -e "${BOLD}NGINX TLS Termination (optional):${NC}"
echo ""
echo -e "  An example NGINX configuration is available at:"
echo -e "     ${TARGET_DIR}/share/doc/nginx-gateway.conf.example"
echo ""
echo -e "  To use it:"
echo -e "     cp ${TARGET_DIR}/share/doc/nginx-gateway.conf.example /etc/nginx/conf.d/rest-api-gateway.conf"
echo -e "     # Edit server_name and SSL certificate paths"
echo -e "     vi /etc/nginx/conf.d/rest-api-gateway.conf"
echo -e "     nginx -t && systemctl reload nginx"
echo ""
echo -e "${BOLD}Security Notes:${NC}"
echo -e "  - The AES-256 key is stored in: ${TARGET_DIR}/etc/gateway.env"
echo -e "  - This file is readable only by root and the ${SERVICE_GROUP} group"
echo -e "  - Do NOT commit this key to version control"
echo ""
