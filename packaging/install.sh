#!/usr/bin/env bash
# Redirect to the root-level install.sh
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "${SCRIPT_DIR}/../install.sh" "$@"
