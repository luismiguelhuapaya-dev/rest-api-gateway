#!/usr/bin/env bash
# Redirect to the root-level uninstall.sh
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "${SCRIPT_DIR}/../uninstall.sh" "$@"
