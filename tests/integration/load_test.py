#!/usr/bin/env python3
"""
Load Testing Tool for the Dynamic REST API Gateway Server.

Sends concurrent HTTP requests and measures throughput and latency
(p50, p95, p99).

Uses only the Python standard library (http.client, threading, time).
"""

import http.client
import json
import time
import threading
import argparse
import sys
import statistics
import socket

# ---------------------------------------------------------------------------
# Worker
# ---------------------------------------------------------------------------

class LoadWorker:
    """Sends a batch of HTTP requests and records their latencies."""

    def __init__(self, worker_id, host, port, requests_per_worker, endpoint,
                 method="GET", body=None, headers=None):
        self.worker_id = worker_id
        self.host = host
        self.port = port
        self.total = requests_per_worker
        self.endpoint = endpoint
        self.method = method
        self.body = body
        self.headers = headers or {}
        self.latencies = []
        self.errors = 0
        self.status_counts = {}

    def run(self):
        for i in range(self.total):
            start = time.perf_counter()
            try:
                conn = http.client.HTTPConnection(self.host, self.port, timeout=10)
                hdrs = {"Content-Type": "application/json", "Accept": "application/json"}
                hdrs.update(self.headers)
                body_bytes = None
                if self.body is not None:
                    body_bytes = json.dumps(self.body).encode("utf-8")
                conn.request(self.method, self.endpoint, body=body_bytes, headers=hdrs)
                resp = conn.getresponse()
                _ = resp.read()
                status = resp.status
                conn.close()
                elapsed = time.perf_counter() - start
                self.latencies.append(elapsed)
                self.status_counts[status] = self.status_counts.get(status, 0) + 1
            except Exception:
                self.errors += 1
                elapsed = time.perf_counter() - start
                self.latencies.append(elapsed)

# ---------------------------------------------------------------------------
# Percentile helpers
# ---------------------------------------------------------------------------

def percentile(data, pct):
    """Compute the *pct*-th percentile of a sorted list."""
    if not data:
        return 0.0
    k = (len(data) - 1) * (pct / 100.0)
    f = int(k)
    c = f + 1 if f + 1 < len(data) else f
    d = k - f
    return data[f] + d * (data[c] - data[f])

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def run_load_test(host, port, concurrency, total_requests, endpoint, method,
                  body, headers):
    requests_per_worker = total_requests // concurrency
    remainder = total_requests % concurrency

    workers = []
    for i in range(concurrency):
        extra = 1 if i < remainder else 0
        w = LoadWorker(
            worker_id=i,
            host=host,
            port=port,
            requests_per_worker=requests_per_worker + extra,
            endpoint=endpoint,
            method=method,
            body=body,
            headers=headers,
        )
        workers.append(w)

    threads = []
    print(f"Starting load test: {total_requests} requests, {concurrency} concurrent workers")
    print(f"Target: {method} {endpoint} -> {host}:{port}")
    print()

    wall_start = time.perf_counter()

    for w in workers:
        t = threading.Thread(target=w.run, daemon=True)
        threads.append(t)
        t.start()

    for t in threads:
        t.join()

    wall_elapsed = time.perf_counter() - wall_start

    # Aggregate results
    all_latencies = []
    total_errors = 0
    all_status = {}
    for w in workers:
        all_latencies.extend(w.latencies)
        total_errors += w.errors
        for s, c in w.status_counts.items():
            all_status[s] = all_status.get(s, 0) + c

    all_latencies.sort()
    completed = len(all_latencies)
    rps = completed / wall_elapsed if wall_elapsed > 0 else 0

    print("=" * 60)
    print("LOAD TEST RESULTS")
    print("=" * 60)
    print(f"  Total requests sent:   {completed}")
    print(f"  Total errors:          {total_errors}")
    print(f"  Wall-clock time:       {wall_elapsed:.3f} s")
    print(f"  Throughput:            {rps:.1f} req/s")
    print()

    if all_latencies:
        print("  Latency (seconds):")
        print(f"    Min:    {all_latencies[0]:.6f}")
        print(f"    p50:    {percentile(all_latencies, 50):.6f}")
        print(f"    p90:    {percentile(all_latencies, 90):.6f}")
        print(f"    p95:    {percentile(all_latencies, 95):.6f}")
        print(f"    p99:    {percentile(all_latencies, 99):.6f}")
        print(f"    Max:    {all_latencies[-1]:.6f}")
        print(f"    Mean:   {statistics.mean(all_latencies):.6f}")
        if len(all_latencies) > 1:
            print(f"    StdDev: {statistics.stdev(all_latencies):.6f}")
    print()

    if all_status:
        print("  HTTP status codes:")
        for status in sorted(all_status.keys()):
            print(f"    {status}: {all_status[status]}")
    print("=" * 60)

    return total_errors == 0


def main():
    parser = argparse.ArgumentParser(
        description="Load test the Dynamic REST API Gateway",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("--host", default="127.0.0.1", help="Gateway host")
    parser.add_argument("--port", "-p", type=int, default=8080, help="Gateway port")
    parser.add_argument("--concurrency", "-c", type=int, default=10,
                        help="Number of concurrent workers")
    parser.add_argument("--requests", "-n", type=int, default=1000,
                        help="Total number of requests")
    parser.add_argument("--endpoint", "-e", default="/api/v1/health",
                        help="Endpoint path to test")
    parser.add_argument("--method", "-m", default="GET",
                        help="HTTP method")
    parser.add_argument("--body", "-b", default=None,
                        help="JSON body (as string)")
    parser.add_argument("--token", "-t", default=None,
                        help="Bearer token for authenticated endpoints")

    args = parser.parse_args()

    body = None
    if args.body:
        try:
            body = json.loads(args.body)
        except json.JSONDecodeError:
            print(f"Error: --body must be valid JSON, got: {args.body}")
            sys.exit(1)

    headers = {}
    if args.token:
        headers["Authorization"] = f"Bearer {args.token}"

    success = run_load_test(
        host=args.host,
        port=args.port,
        concurrency=args.concurrency,
        total_requests=args.requests,
        endpoint=args.endpoint,
        method=args.method,
        body=body,
        headers=headers,
    )
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
