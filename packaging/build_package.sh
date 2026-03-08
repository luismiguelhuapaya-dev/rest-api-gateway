#!/usr/bin/env bash
###############################################################################
# build_package.sh - Create distributable tar.gz package
#
# Usage: ./build_package.sh [OUTPUT_DIR]
#   OUTPUT_DIR defaults to ./dist
#
# Creates:
#   rest-api-gateway-<VERSION>.tar.gz
#   rest-api-gateway-<VERSION>.tar.gz.sha256
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

print_error() {
    echo -e "  ${RED}[ERROR]${NC} $1"
}

print_info() {
    echo -e "  ${BLUE}[INFO]${NC} $1"
}

# ─── Configuration ───────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
OUTPUT_DIR="${1:-${SOURCE_DIR}/dist}"

# Read version
VERSION="unknown"
if [[ -f "${SCRIPT_DIR}/VERSION" ]]; then
    VERSION="$(cat "${SCRIPT_DIR}/VERSION" | tr -d '[:space:]')"
elif [[ -f "${SOURCE_DIR}/packaging/VERSION" ]]; then
    VERSION="$(cat "${SOURCE_DIR}/packaging/VERSION" | tr -d '[:space:]')"
fi

PACKAGE_NAME="rest-api-gateway-${VERSION}"
TARBALL="${PACKAGE_NAME}.tar.gz"

echo ""
echo -e "${BOLD}${BLUE}================================================================${NC}"
echo -e "${BOLD}${BLUE}  Dynamic REST API Gateway Server - Package Builder${NC}"
echo -e "${BOLD}${BLUE}================================================================${NC}"
echo ""
print_info "Version:     ${VERSION}"
print_info "Source:      ${SOURCE_DIR}"
print_info "Output:      ${OUTPUT_DIR}"
print_info "Package:     ${TARBALL}"
echo ""

# ─── Validate source tree ───────────────────────────────────────────────────
echo -e "${BOLD}${CYAN}Validating source tree...${NC}"

REQUIRED_FILES=(
    "CMakeLists.txt"
    "src/main.cpp"
    "packaging/install.sh"
    "packaging/uninstall.sh"
    "packaging/VERSION"
)

VALID=true
for file in "${REQUIRED_FILES[@]}"; do
    if [[ -f "${SOURCE_DIR}/${file}" ]]; then
        print_ok "Found: ${file}"
    else
        print_error "Missing: ${file}"
        VALID=false
    fi
done

if [[ "${VALID}" != "true" ]]; then
    print_error "Source tree validation failed. Cannot build package."
    exit 1
fi

# ─── Create staging directory ────────────────────────────────────────────────
echo ""
echo -e "${BOLD}${CYAN}Creating staging directory...${NC}"

STAGING_DIR="$(mktemp -d /tmp/rest-api-gateway-pkg.XXXXXX)"
STAGING_PKG="${STAGING_DIR}/${PACKAGE_NAME}"
mkdir -p "${STAGING_PKG}"
print_ok "Staging directory: ${STAGING_DIR}"

# Error handler to clean up staging directory
cleanup() {
    local exit_code=$?
    if [[ -d "${STAGING_DIR}" ]]; then
        rm -rf "${STAGING_DIR}"
    fi
    if [[ ${exit_code} -ne 0 ]]; then
        print_error "Package build failed."
    fi
}
trap cleanup EXIT

# ─── Copy source files ──────────────────────────────────────────────────────
echo ""
echo -e "${BOLD}${CYAN}Copying source files...${NC}"

# Source code
cp -r "${SOURCE_DIR}/src" "${STAGING_PKG}/src"
print_ok "Copied src/"

cp -r "${SOURCE_DIR}/include" "${STAGING_PKG}/include"
print_ok "Copied include/"

# Build system
cp "${SOURCE_DIR}/CMakeLists.txt" "${STAGING_PKG}/CMakeLists.txt"
print_ok "Copied CMakeLists.txt"

# Packaging scripts and config
cp -r "${SOURCE_DIR}/packaging" "${STAGING_PKG}/packaging"
chmod +x "${STAGING_PKG}/packaging/install.sh"
chmod +x "${STAGING_PKG}/packaging/uninstall.sh"
chmod +x "${STAGING_PKG}/packaging/build_package.sh"
print_ok "Copied packaging/"

# Tests
if [[ -d "${SOURCE_DIR}/tests" ]]; then
    cp -r "${SOURCE_DIR}/tests" "${STAGING_PKG}/tests"
    print_ok "Copied tests/"
else
    print_info "No tests/ directory found, skipping"
fi

# Documentation
if [[ -f "${SOURCE_DIR}/README.md" ]]; then
    cp "${SOURCE_DIR}/README.md" "${STAGING_PKG}/README.md"
    print_ok "Copied README.md"
fi

if [[ -d "${SOURCE_DIR}/docs" ]]; then
    cp -r "${SOURCE_DIR}/docs" "${STAGING_PKG}/docs"
    print_ok "Copied docs/"
fi

# Coding Standards
if [[ -d "${SOURCE_DIR}/Coding Standards" ]]; then
    cp -r "${SOURCE_DIR}/Coding Standards" "${STAGING_PKG}/Coding Standards"
    print_ok "Copied Coding Standards/"
fi

# .gitignore (useful for development)
if [[ -f "${SOURCE_DIR}/.gitignore" ]]; then
    cp "${SOURCE_DIR}/.gitignore" "${STAGING_PKG}/.gitignore"
    print_ok "Copied .gitignore"
fi

# ─── Create tar.gz package ──────────────────────────────────────────────────
echo ""
echo -e "${BOLD}${CYAN}Building package...${NC}"

mkdir -p "${OUTPUT_DIR}"

tar -czf "${OUTPUT_DIR}/${TARBALL}" \
    -C "${STAGING_DIR}" \
    "${PACKAGE_NAME}"

TARBALL_SIZE="$(du -h "${OUTPUT_DIR}/${TARBALL}" | cut -f1)"
print_ok "Created: ${OUTPUT_DIR}/${TARBALL} (${TARBALL_SIZE})"

# ─── Create SHA256 checksum ─────────────────────────────────────────────────
echo ""
echo -e "${BOLD}${CYAN}Generating checksum...${NC}"

(cd "${OUTPUT_DIR}" && sha256sum "${TARBALL}" > "${TARBALL}.sha256")
CHECKSUM="$(cat "${OUTPUT_DIR}/${TARBALL}.sha256" | cut -d' ' -f1)"
print_ok "SHA256: ${CHECKSUM}"
print_ok "Checksum file: ${OUTPUT_DIR}/${TARBALL}.sha256"

# ─── List package contents ──────────────────────────────────────────────────
echo ""
echo -e "${BOLD}${CYAN}Package contents:${NC}"
tar -tzf "${OUTPUT_DIR}/${TARBALL}" | head -40
FILE_COUNT="$(tar -tzf "${OUTPUT_DIR}/${TARBALL}" | wc -l)"
if [[ "${FILE_COUNT}" -gt 40 ]]; then
    echo "  ... and $((FILE_COUNT - 40)) more files"
fi

# ─── Clean up ────────────────────────────────────────────────────────────────
rm -rf "${STAGING_DIR}"
# Prevent cleanup trap from running on already-deleted dir
STAGING_DIR=""

# ─── Summary ─────────────────────────────────────────────────────────────────
echo ""
echo -e "${BOLD}${GREEN}================================================================${NC}"
echo -e "${BOLD}${GREEN}  Package Built Successfully${NC}"
echo -e "${BOLD}${GREEN}================================================================${NC}"
echo ""
echo -e "  Package:   ${OUTPUT_DIR}/${TARBALL}"
echo -e "  Checksum:  ${OUTPUT_DIR}/${TARBALL}.sha256"
echo -e "  Size:      ${TARBALL_SIZE}"
echo -e "  SHA256:    ${CHECKSUM}"
echo ""
echo -e "${BOLD}Deployment Instructions:${NC}"
echo ""
echo -e "  1. Copy the package to the target server:"
echo -e "     scp ${OUTPUT_DIR}/${TARBALL} user@server:/tmp/"
echo ""
echo -e "  2. Verify the checksum:"
echo -e "     sha256sum -c ${TARBALL}.sha256"
echo ""
echo -e "  3. Extract and install:"
echo -e "     tar xzf ${TARBALL}"
echo -e "     cd ${PACKAGE_NAME}"
echo -e "     sudo ./packaging/install.sh /opt/rest-api-gateway"
echo ""
