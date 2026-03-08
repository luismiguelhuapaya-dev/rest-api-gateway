#!/usr/bin/env bash
###############################################################################
# uninstall.sh - Dynamic REST API Gateway Server Uninstaller
#
# Usage: ./uninstall.sh [INSTALL_DIR]
#   INSTALL_DIR defaults to /opt/rest-api-gateway
#
# Options:
#   --remove-user    Also remove the 'gateway' service user
#   --force          Skip confirmation prompts
#   --help           Show usage information
###############################################################################

set -euo pipefail

# ─── Color codes and formatting ──────────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

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

# ─── Parse arguments ────────────────────────────────────────────────────────
INSTALL_DIR=""
REMOVE_USER=false
FORCE=false

show_help() {
    echo "Usage: $0 [OPTIONS] [INSTALL_DIR]"
    echo ""
    echo "Uninstall the Dynamic REST API Gateway Server."
    echo ""
    echo "Arguments:"
    echo "  INSTALL_DIR     Installation directory (default: /opt/rest-api-gateway)"
    echo ""
    echo "Options:"
    echo "  --remove-user   Also remove the 'gateway' service user"
    echo "  --force         Skip confirmation prompts"
    echo "  --help          Show this help message"
    echo ""
}

for arg in "$@"; do
    case "${arg}" in
        --remove-user)
            REMOVE_USER=true
            ;;
        --force)
            FORCE=true
            ;;
        --help|-h)
            show_help
            exit 0
            ;;
        -*)
            print_error "Unknown option: ${arg}"
            show_help
            exit 1
            ;;
        *)
            if [[ -z "${INSTALL_DIR}" ]]; then
                INSTALL_DIR="${arg}"
            else
                print_error "Unexpected argument: ${arg}"
                show_help
                exit 1
            fi
            ;;
    esac
done

INSTALL_DIR="${INSTALL_DIR:-/opt/rest-api-gateway}"
SERVICE_USER="gateway"

# ─── Pre-flight checks ──────────────────────────────────────────────────────
echo ""
echo -e "${BOLD}${RED}================================================================${NC}"
echo -e "${BOLD}${RED}  Dynamic REST API Gateway Server - Uninstaller${NC}"
echo -e "${BOLD}${RED}================================================================${NC}"
echo ""

# Must be running on Linux
if [[ "$(uname -s)" != "Linux" ]]; then
    print_error "This uninstaller only supports Linux systems."
    exit 1
fi

# Must be root
if [[ "$(id -u)" -ne 0 ]]; then
    print_error "This uninstaller must be run as root (or with sudo)."
    exit 1
fi

# Check installation exists
if [[ ! -d "${INSTALL_DIR}" ]]; then
    print_error "Installation directory not found: ${INSTALL_DIR}"
    exit 1
fi

print_info "Installation directory: ${INSTALL_DIR}"

# ─── Confirmation ───────────────────────────────────────────────────────────
if [[ "${FORCE}" != "true" ]]; then
    echo ""
    echo -e "${YELLOW}This will:${NC}"
    echo "  - Stop the rest-api-gateway service (if running)"
    echo "  - Remove the systemd service file"
    echo "  - Remove the logrotate configuration"
    echo "  - Delete the installation directory: ${INSTALL_DIR}"
    if [[ "${REMOVE_USER}" == "true" ]]; then
        echo "  - Remove the '${SERVICE_USER}' system user"
    fi
    echo ""
    read -r -p "Are you sure you want to continue? [y/N] " response
    if [[ ! "${response}" =~ ^[Yy]$ ]]; then
        echo "Uninstallation cancelled."
        exit 0
    fi
fi

# ─── Stop service ───────────────────────────────────────────────────────────
echo ""
echo -e "${BOLD}${CYAN}Stopping service...${NC}"

if command -v systemctl &>/dev/null; then
    if systemctl is-active --quiet rest-api-gateway 2>/dev/null; then
        systemctl stop rest-api-gateway
        print_ok "Service stopped"
    else
        print_info "Service is not running"
    fi

    if systemctl is-enabled --quiet rest-api-gateway 2>/dev/null; then
        systemctl disable rest-api-gateway
        print_ok "Service disabled"
    else
        print_info "Service is not enabled"
    fi
else
    # Fallback: try to kill process directly
    if [[ -f "${INSTALL_DIR}/run/gateway.pid" ]]; then
        PID="$(cat "${INSTALL_DIR}/run/gateway.pid")"
        if kill -0 "${PID}" 2>/dev/null; then
            kill "${PID}"
            sleep 2
            # Force kill if still running
            if kill -0 "${PID}" 2>/dev/null; then
                kill -9 "${PID}"
            fi
            print_ok "Process stopped (PID: ${PID})"
        fi
    fi
fi

# ─── Remove systemd service file ────────────────────────────────────────────
echo ""
echo -e "${BOLD}${CYAN}Removing systemd service...${NC}"

if [[ -f /etc/systemd/system/rest-api-gateway.service ]]; then
    rm -f /etc/systemd/system/rest-api-gateway.service
    if command -v systemctl &>/dev/null; then
        systemctl daemon-reload
    fi
    print_ok "Removed systemd service file"
else
    print_info "Systemd service file not found (already removed)"
fi

# ─── Remove logrotate configuration ─────────────────────────────────────────
echo ""
echo -e "${BOLD}${CYAN}Removing logrotate configuration...${NC}"

if [[ -f /etc/logrotate.d/rest-api-gateway ]]; then
    rm -f /etc/logrotate.d/rest-api-gateway
    print_ok "Removed logrotate configuration"
else
    print_info "Logrotate configuration not found (already removed)"
fi

# ─── Remove installation directory ──────────────────────────────────────────
echo ""
echo -e "${BOLD}${CYAN}Removing installation directory...${NC}"

if [[ -d "${INSTALL_DIR}" ]]; then
    rm -rf "${INSTALL_DIR}"
    print_ok "Removed ${INSTALL_DIR}"
else
    print_info "Installation directory already removed"
fi

# ─── Remove service user (optional) ─────────────────────────────────────────
if [[ "${REMOVE_USER}" == "true" ]]; then
    echo ""
    echo -e "${BOLD}${CYAN}Removing service user...${NC}"

    if id "${SERVICE_USER}" &>/dev/null; then
        userdel "${SERVICE_USER}" 2>/dev/null || true
        print_ok "Removed user '${SERVICE_USER}'"

        # Remove group if it still exists and is empty
        if getent group "${SERVICE_USER}" &>/dev/null; then
            groupdel "${SERVICE_USER}" 2>/dev/null || true
            print_ok "Removed group '${SERVICE_USER}'"
        fi
    else
        print_info "User '${SERVICE_USER}' does not exist (already removed)"
    fi
else
    echo ""
    print_info "Service user '${SERVICE_USER}' was NOT removed."
    print_info "To remove it, run: userdel ${SERVICE_USER}"
fi

# ─── Done ────────────────────────────────────────────────────────────────────
echo ""
echo -e "${BOLD}${GREEN}================================================================${NC}"
echo -e "${BOLD}${GREEN}  Uninstallation Complete${NC}"
echo -e "${BOLD}${GREEN}================================================================${NC}"
echo ""
echo -e "The Dynamic REST API Gateway Server has been removed from ${INSTALL_DIR}."
echo ""
