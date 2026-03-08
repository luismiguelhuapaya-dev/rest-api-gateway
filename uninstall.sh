#!/usr/bin/env bash
###############################################################################
# uninstall.sh — Dynamic REST API Gateway Server
#
# Usage:
#   sudo ./uninstall.sh <INSTALL_DIR>
#
# Example:
#   sudo ./uninstall.sh /opt/rest-api-gateway
###############################################################################

set -euo pipefail

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[0;33m'
BOLD='\033[1m'; NC='\033[0m'

ok()   { echo -e "  ${GREEN}✓${NC} $1"; }
warn() { echo -e "  ${YELLOW}!${NC} $1"; }
err()  { echo -e "  ${RED}✗${NC} $1"; }

if [[ $# -lt 1 ]] || [[ "$1" == "--help" ]]; then
    echo "Usage: sudo $0 <INSTALL_DIR>"
    exit 0
fi

INSTALL_DIR="$1"
SERVICE_USER="gateway"

[[ "$(uname -s)" == "Linux" ]] || { err "Linux only."; exit 1; }
[[ "$(id -u)" -eq 0 ]]        || { err "Must run as root."; exit 1; }
[[ -d "${INSTALL_DIR}" ]]     || { err "Not found: ${INSTALL_DIR}"; exit 1; }

echo ""
echo -e "${BOLD}${RED}Uninstalling REST API Gateway from ${INSTALL_DIR}${NC}"
echo ""

# Stop and disable service
if systemctl is-active --quiet rest-api-gateway 2>/dev/null; then
    systemctl stop rest-api-gateway
    ok "Service stopped"
fi
if systemctl is-enabled --quiet rest-api-gateway 2>/dev/null; then
    systemctl disable rest-api-gateway --quiet 2>/dev/null
    ok "Service disabled"
fi

# Remove systemd unit
rm -f /etc/systemd/system/rest-api-gateway.service
systemctl daemon-reload 2>/dev/null || true
ok "systemd service removed"

# Remove logrotate
rm -f /etc/logrotate.d/rest-api-gateway
ok "logrotate config removed"

# Remove install directory
rm -rf "${INSTALL_DIR}"
ok "Removed ${INSTALL_DIR}"

# Remove service user (optional — leave it if other things depend on it)
if id "${SERVICE_USER}" &>/dev/null; then
    userdel "${SERVICE_USER}" 2>/dev/null || true
    ok "Removed user '${SERVICE_USER}'"
fi

echo ""
echo -e "${BOLD}${GREEN}Uninstallation complete.${NC}"
echo ""
