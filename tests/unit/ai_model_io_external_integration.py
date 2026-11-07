"""Cross-process C++ D328 client to Python D329 server acceptance test."""

from __future__ import annotations

import socket
import subprocess
import sys
from pathlib import Path


def run_server(server: Path, client: Path, mode: str, frames: tuple[tuple[int, str], ...],
               *, fault: str = "none", fault_on_request: int = 1):
    process = subprocess.Popen(
        [sys.executable, str(server), "--port", "0", "--max-connections",
         str(len(frames)), "--mode", mode, "--fault", fault,
         "--fault-on-request", str(fault_on_request)],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
    )
    try:
        if process.stdout is None:
            raise AssertionError("server stdout unavailable")
        ready = process.stdout.readline().strip()
        if not ready.startswith("READY "):
            raise AssertionError("server startup failed: " + ready)
        port = int(ready.split()[1])
        if not 1 <= port <= 65535:
            raise AssertionError("invalid server endpoint")

        for frame, expected in frames:
            result = subprocess.run(
                [str(client), str(port), str(frame), expected, "106.0"],
                capture_output=True, text=True, timeout=5, check=False,
            )
            if result.returncode != 0:
                raise AssertionError(
                    f"frame={frame} expected={expected} rc={result.returncode}"
                    f"\nstdout={result.stdout}\nstderr={result.stderr}"
                )
        if process.wait(timeout=5) != 0:
            raise AssertionError("test server did not exit after bounded connections")
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=5)
        if process.stdout is not None:
            process.stdout.close()
        if process.stderr is not None:
            process.stderr.close()


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: ai_model_io_external_integration.py SERVER CLIENT", file=sys.stderr)
        return 2
    server = Path(sys.argv[1]).resolve()
    client = Path(sys.argv[2]).resolve()
    if not server.is_file() or not client.is_file():
        raise AssertionError("integration executables are missing")
    # Separate Python server process, real TCP socket, D326 wire and D327
    # backend all execute here; the C++ client is independent of Python.
    run_server(server, client, "no-decision", ((12, "none"),))
    run_server(server, client, "move-once", ((12, "move"), (13, "none")))
    for fault in ("disconnect", "timeout", "truncated", "bad-magic",
                  "bad-version", "oversized", "foreign-request",
                  "foreign-session", "foreign-frame", "nonfinite"):
        run_server(server, client, "move-once", ((12, "transport-error"),),
                   fault=fault)
    for fault in ("unsafe-target", "unsafe-action", "no-decision"):
        run_server(server, client, "move-once", ((12, "guard-error"),),
                   fault=fault)
    print("PASS: C++ Model I/O client to Python server, plus 13 fail-closed fault scenarios")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
