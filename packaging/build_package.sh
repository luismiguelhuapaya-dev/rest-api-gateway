#!/usr/bin/env bash
###############################################################################
# build_package.sh — Create the distributable tar.gz
#
# Usage: ./packaging/build_package.sh [OUTPUT_DIR]
#   OUTPUT_DIR defaults to ./dist
#
# Produces:
#   rest-api-gateway-<VERSION>.tar.gz
#   rest-api-gateway-<VERSION>.tar.gz.sha256
#
# The tar extracts to:
#   rest-api-gateway-<VERSION>/
#     install.sh          ← run this as root
#     uninstall.sh
#     CMakeLists.txt
#     src/
#     include/
#     packaging/          ← config templates, service files
#     docs/
#     tests/
#     README.md
###############################################################################

set -euo pipefail

GREEN='\033[0;32m'; BLUE='\033[0;34m'; BOLD='\033[1m'; NC='\033[0m'
ok()   { echo -e "  ${GREEN}✓${NC} $1"; }
info() { echo -e "  ${BLUE}→${NC} $1"; }

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

# Copy files — install.sh and uninstall.sh at the ROOT of the package
cp "${SRC_ROOT}/install.sh"   "${PKG_DIR}/install.sh"
cp "${SRC_ROOT}/uninstall.sh" "${PKG_DIR}/uninstall.sh"
chmod +x "${PKG_DIR}/install.sh" "${PKG_DIR}/uninstall.sh"
ok "install.sh / uninstall.sh (package root)"

cp    "${SRC_ROOT}/CMakeLists.txt" "${PKG_DIR}/"
cp -r "${SRC_ROOT}/src"            "${PKG_DIR}/"
cp -r "${SRC_ROOT}/include"        "${PKG_DIR}/"
ok "Source code (src/, include/, CMakeLists.txt)"

cp -r "${SRC_ROOT}/packaging" "${PKG_DIR}/"
ok "Packaging configs"

[[ -f "${SRC_ROOT}/README.md" ]] && cp "${SRC_ROOT}/README.md" "${PKG_DIR}/"
[[ -d "${SRC_ROOT}/docs" ]]      && cp -r "${SRC_ROOT}/docs" "${PKG_DIR}/"
ok "Documentation"

[[ -d "${SRC_ROOT}/tests" ]]              && cp -r "${SRC_ROOT}/tests" "${PKG_DIR}/"
[[ -d "${SRC_ROOT}/Coding Standards" ]]   && cp -r "${SRC_ROOT}/Coding Standards" "${PKG_DIR}/"
ok "Tests and coding standards"

# Build tar
mkdir -p "${OUTPUT_DIR}"
tar -czf "${OUTPUT_DIR}/${TARBALL}" -C "${STAGING}" "${PKG}"
ok "Created ${OUTPUT_DIR}/${TARBALL}"

# Checksum
(cd "${OUTPUT_DIR}" && sha256sum "${TARBALL}" > "${TARBALL}.sha256")
HASH="$(cut -d' ' -f1 < "${OUTPUT_DIR}/${TARBALL}.sha256")"
ok "SHA256: ${HASH}"

SIZE="$(du -h "${OUTPUT_DIR}/${TARBALL}" | cut -f1)"

echo ""
echo -e "${BOLD}${GREEN}Package ready: ${OUTPUT_DIR}/${TARBALL} (${SIZE})${NC}"
echo ""
echo "  Deploy to any Ubuntu/Debian server:"
echo ""
echo "    scp ${OUTPUT_DIR}/${TARBALL} user@server:/tmp/"
echo "    ssh user@server 'cd /tmp && tar xzf ${TARBALL} && sudo ./${PKG}/install.sh /opt/rest-api-gateway'"
echo ""
