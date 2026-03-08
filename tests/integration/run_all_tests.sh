#!/usr/bin/env bash
# ==========================================================================
# Integration Test Runner for the Dynamic REST API Gateway Server
#
# Orchestrates:
#   1. Build the gateway (if needed)
#   2. Start the gateway with test configuration
#   3. Start the backend simulator
#   4. Run HTTP client tests
#   5. Run network scan test
#   6. Tear down and report results
# ==========================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Configuration
GATEWAY_PORT="${GATEWAY_PORT:-8080}"
GATEWAY_HOST="${GATEWAY_HOST:-127.0.0.1}"
SOCKET_PATH="${SOCKET_PATH:-/tmp/gateway_test.sock}"
CONFIG_FILE="$SCRIPT_DIR/test_config.json"
BUILD_DIR="$PROJECT_ROOT/build"
GATEWAY_BIN="$BUILD_DIR/gateway"
LOG_FILE="/tmp/gateway_test.log"
AES_KEY="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"

# PIDs to clean up
GATEWAY_PID=""
BACKEND_PID=""

# Test result tracking
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# --------------------------------------------------------------------------
# Utility functions
# --------------------------------------------------------------------------

cleanup() {
    echo ""
    echo "=== Cleaning up ==="

    if [[ -n "$BACKEND_PID" ]] && kill -0 "$BACKEND_PID" 2>/dev/null; then
        echo "  Stopping backend simulator (PID $BACKEND_PID)..."
        kill "$BACKEND_PID" 2>/dev/null || true
        wait "$BACKEND_PID" 2>/dev/null || true
    fi

    if [[ -n "$GATEWAY_PID" ]] && kill -0 "$GATEWAY_PID" 2>/dev/null; then
        echo "  Stopping gateway server (PID $GATEWAY_PID)..."
        kill "$GATEWAY_PID" 2>/dev/null || true
        wait "$GATEWAY_PID" 2>/dev/null || true
    fi

    # Remove socket file
    rm -f "$SOCKET_PATH"
    echo "  Cleanup complete."
}

trap cleanup EXIT

wait_for_port() {
    local host="$1"
    local port="$2"
    local timeout="${3:-15}"
    local elapsed=0

    echo -n "  Waiting for $host:$port "
    while ! (echo >/dev/tcp/"$host"/"$port") 2>/dev/null; do
        sleep 0.5
        elapsed=$((elapsed + 1))
        echo -n "."
        if [[ $elapsed -ge $((timeout * 2)) ]]; then
            echo " TIMEOUT"
            return 1
        fi
    done
    echo " ready"
    return 0
}

wait_for_socket() {
    local path="$1"
    local timeout="${2:-15}"
    local elapsed=0

    echo -n "  Waiting for Unix socket $path "
    while [[ ! -S "$path" ]]; do
        sleep 0.5
        elapsed=$((elapsed + 1))
        echo -n "."
        if [[ $elapsed -ge $((timeout * 2)) ]]; then
            echo " TIMEOUT"
            return 1
        fi
    done
    echo " ready"
    return 0
}

record_test() {
    local name="$1"
    local exit_code="$2"
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    if [[ "$exit_code" -eq 0 ]]; then
        PASSED_TESTS=$((PASSED_TESTS + 1))
        echo "  [PASS] $name"
    else
        FAILED_TESTS=$((FAILED_TESTS + 1))
        echo "  [FAIL] $name"
    fi
}

# --------------------------------------------------------------------------
# Step 1: Build
# --------------------------------------------------------------------------

echo "======================================================================"
echo "  Dynamic REST API Gateway -- Integration Test Suite"
echo "======================================================================"
echo ""

echo "=== Step 1: Build ==="

if [[ -f "$GATEWAY_BIN" ]]; then
    echo "  Gateway binary found at $GATEWAY_BIN, skipping build."
else
    echo "  Building gateway..."
    mkdir -p "$BUILD_DIR"
    if command -v cmake &>/dev/null; then
        (cd "$BUILD_DIR" && cmake .. && cmake --build . --config Release -j"$(nproc 2>/dev/null || echo 4)")
    elif command -v make &>/dev/null; then
        (cd "$BUILD_DIR" && make -C "$PROJECT_ROOT" -j"$(nproc 2>/dev/null || echo 4)")
    else
        echo "  [WARN] No cmake or make found. Attempting direct g++ compilation..."
        g++ -std=c++20 -O2 \
            -I"$PROJECT_ROOT/include" \
            "$PROJECT_ROOT"/src/**/*.cpp \
            "$PROJECT_ROOT"/src/*.cpp \
            -o "$GATEWAY_BIN" \
            -lpthread 2>/dev/null || {
            echo "  [ERROR] Build failed. Please build manually and re-run."
            exit 1
        }
    fi

    if [[ -f "$GATEWAY_BIN" ]]; then
        echo "  Build successful."
    else
        echo "  [ERROR] Build did not produce $GATEWAY_BIN"
        echo "  Please build the gateway manually and re-run this script."
        exit 1
    fi
fi

# --------------------------------------------------------------------------
# Step 2: Start the gateway
# --------------------------------------------------------------------------

echo ""
echo "=== Step 2: Start Gateway ==="

# Remove stale socket
rm -f "$SOCKET_PATH"

export GATEWAY_AES_KEY="$AES_KEY"

"$GATEWAY_BIN" --config "$CONFIG_FILE" \
    --port "$GATEWAY_PORT" \
    --socket "$SOCKET_PATH" \
    --log-level debug \
    --log-stdout \
    > "$LOG_FILE" 2>&1 &
GATEWAY_PID=$!
echo "  Gateway started (PID $GATEWAY_PID)"

# Wait for both listeners
if ! wait_for_port "$GATEWAY_HOST" "$GATEWAY_PORT" 15; then
    echo "  [ERROR] Gateway TCP listener did not start"
    cat "$LOG_FILE" | tail -20
    exit 1
fi

if ! wait_for_socket "$SOCKET_PATH" 15; then
    echo "  [ERROR] Gateway Unix socket listener did not start"
    cat "$LOG_FILE" | tail -20
    exit 1
fi

# --------------------------------------------------------------------------
# Step 3: Start backend simulator
# --------------------------------------------------------------------------

echo ""
echo "=== Step 3: Start Backend Simulator ==="

python3 "$SCRIPT_DIR/backend_simulator.py" \
    --socket "$SOCKET_PATH" \
    --backend-id "test_backend_v1" \
    --verbose &
BACKEND_PID=$!
echo "  Backend simulator started (PID $BACKEND_PID)"
sleep 1  # Give it time to register

if ! kill -0 "$BACKEND_PID" 2>/dev/null; then
    echo "  [ERROR] Backend simulator exited prematurely"
    exit 1
fi
echo "  Backend simulator running."

# --------------------------------------------------------------------------
# Step 4: Run HTTP client tests
# --------------------------------------------------------------------------

echo ""
echo "=== Step 4: HTTP Client Tests ==="

set +e
python3 "$SCRIPT_DIR/http_client.py" \
    --host "$GATEWAY_HOST" \
    --port "$GATEWAY_PORT"
HTTP_EXIT=$?
set -e

record_test "HTTP Client Test Suite" "$HTTP_EXIT"

# --------------------------------------------------------------------------
# Step 5: Run network scan test
# --------------------------------------------------------------------------

echo ""
echo "=== Step 5: Network Scan Test ==="

set +e
python3 "$SCRIPT_DIR/network_scan_test.py" --timeout 5
SCAN_EXIT=$?
set -e

record_test "Network Scan Test" "$SCAN_EXIT"

# --------------------------------------------------------------------------
# Step 6: Summary
# --------------------------------------------------------------------------

echo ""
echo "======================================================================"
echo "  INTEGRATION TEST SUMMARY"
echo "======================================================================"
echo "  Test suites run:   $TOTAL_TESTS"
echo "  Passed:            $PASSED_TESTS"
echo "  Failed:            $FAILED_TESTS"
echo ""

if [[ "$FAILED_TESTS" -gt 0 ]]; then
    echo "  RESULT: SOME TESTS FAILED"
    echo ""
    echo "  Gateway log tail:"
    tail -30 "$LOG_FILE" 2>/dev/null || true
    echo "======================================================================"
    exit 1
else
    echo "  RESULT: ALL TESTS PASSED"
    echo "======================================================================"
    exit 0
fi
