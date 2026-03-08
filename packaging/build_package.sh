#!/usr/bin/env bash
###############################################################################
# build_package.sh — Create the distributable package
#
# Usage: ./packaging/build_package.sh [OUTPUT_DIR]
#   OUTPUT_DIR defaults to ./dist
#
# Produces two files in dist/:
#   install.sh                          ← copy this + tar to server, run it
#   rest-api-gateway-<VERSION>.tar.gz   ← source + configs + docs + tests
###############################################################################

set -euo pipefail

GREEN='\033[0;32m'; BLUE='\033[0;34m'; BOLD='\033[1m'; NC='\033[0m'
ok()   { echo -e "  ${GREEN}✓${NC} $1"; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
OUTPUT_DIR="${1:-${SRC_ROOT}/dist}"

VERSION="1.0.0"
[[ -f "${SCRIPT_DIR}/VERSION" ]] && VERSION="$(tr -d '[:space:]' < "${SCRIPT_DIR}/VERSION")"

PKG="rest-api-gateway-${VERSION}"
TARBALL="${PKG}.tar.gz"

echo ""
echo -e "${BOLD}${BLUE}Building ${TARBALL}${NC}"
echo ""

# Stage into a temp directory
STAGING="$(mktemp -d /tmp/gw-pkg.XXXXXX)"
PKG_DIR="${STAGING}/${PKG}"
mkdir -p "${PKG_DIR}"
trap 'rm -rf "${STAGING}"' EXIT

# Source code + build system
cp    "${SRC_ROOT}/CMakeLists.txt" "${PKG_DIR}/"
cp -r "${SRC_ROOT}/src"            "${PKG_DIR}/"
cp -r "${SRC_ROOT}/include"        "${PKG_DIR}/"
ok "Source code"

# Packaging configs (service file, nginx example, logrotate, templates)
cp -r "${SRC_ROOT}/packaging" "${PKG_DIR}/"
ok "Packaging configs"

# Uninstall script (lives inside the tar)
cp "${SRC_ROOT}/uninstall.sh" "${PKG_DIR}/uninstall.sh"
chmod +x "${PKG_DIR}/uninstall.sh"
ok "uninstall.sh"

# Documentation
[[ -f "${SRC_ROOT}/README.md" ]] && cp "${SRC_ROOT}/README.md" "${PKG_DIR}/"
[[ -d "${SRC_ROOT}/docs" ]]      && cp -r "${SRC_ROOT}/docs" "${PKG_DIR}/"
ok "Documentation"

# Tests + coding standards
[[ -d "${SRC_ROOT}/tests" ]]            && cp -r "${SRC_ROOT}/tests" "${PKG_DIR}/"
[[ -d "${SRC_ROOT}/Coding Standards" ]] && cp -r "${SRC_ROOT}/Coding Standards" "${PKG_DIR}/"
ok "Tests and coding standards"

# Build the tar
mkdir -p "${OUTPUT_DIR}"
rm -f "${OUTPUT_DIR}/${TARBALL}" "${OUTPUT_DIR}/install.sh"
tar -czf "${OUTPUT_DIR}/${TARBALL}" -C "${STAGING}" "${PKG}"
ok "Created ${TARBALL}"

# Copy install.sh alongside the tar
cp "${SRC_ROOT}/install.sh" "${OUTPUT_DIR}/install.sh"
chmod +x "${OUTPUT_DIR}/install.sh"
ok "Copied install.sh"

SIZE="$(du -h "${OUTPUT_DIR}/${TARBALL}" | cut -f1)"

echo ""
echo -e "${BOLD}${GREEN}Done. Two files in ${OUTPUT_DIR}/:${NC}"
echo "  install.sh    (the installer)"
echo "  ${TARBALL}    (${SIZE})"
echo ""
echo "  Deploy:"
echo "    scp ${OUTPUT_DIR}/install.sh ${OUTPUT_DIR}/${TARBALL} user@server:/tmp/"
echo "    ssh user@server 'cd /tmp && sudo ./install.sh /opt/rest-api-gateway'"
echo ""
