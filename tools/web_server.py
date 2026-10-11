#!/usr/bin/env python3
"""Small local bridge between the browser and the C lift simulator.

The bridge validates form data, writes one deterministic scenario, and invokes
the C executable without a shell. Scheduling and metrics remain entirely in C.
"""

from __future__ import annotations

import argparse
import json
import os
import random
import subprocess
import tempfile
from http import HTTPStatus
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
LIMITS = {
    "people": (2, 64, 18),
    "floors": (3, 12, 8),
    "lifts": (1, 4, 2),
    "capacity": (2, 8, 4),
    "seed": (0, 999999, 42),
}


def validated(raw: dict[str, Any]) -> dict[str, int]:
    """Return only known integer settings and reject out-of-range values."""
    clean: dict[str, int] = {}
    for name, (low, high, default) in LIMITS.items():
        value = raw.get(name, default)
        if isinstance(value, bool) or not isinstance(value, int) or not low <= value <= high:
            raise ValueError(f"{name} must be between {low} and {high}")
        clean[name] = value
    return clean


def scenario_text(settings: dict[str, int]) -> str:
    """Create the shared deterministic passenger input for both C policies."""
    people, floors = settings["people"], settings["floors"]
    lifts, capacity, seed = settings["lifts"], settings["capacity"], settings["seed"]
    rng = random.Random(seed)
    starts = [0 if lifts == 1 else round(i * (floors - 1) / (lifts - 1)) for i in range(lifts)]
    lines = [f"@floors={floors}", f"@lifts={lifts}", f"@capacity={capacity}",
             "@start=" + ",".join(map(str, starts)), "# id,arrival,from,to"]
    span = max(8, min(42, people + floors))
    for index in range(people):
        arrival = int(index * span / people + rng.random() * 2)
        mode = rng.random()
        if mode < 0.62:
            origin, target = 0, rng.randrange(1, floors)
        elif mode < 0.82:
            origin, target = rng.randrange(1, floors), 0
        else:
            origin = rng.randrange(floors)
            target = rng.randrange(floors - 1)
            if target >= origin:
                target += 1
        lines.append(f"{index + 1},{arrival},{origin},{target}")
    return "\n".join(lines) + "\n"


def simulate(settings: dict[str, int]) -> dict[str, Any]:
    """Run both policies through the compiled C controller and load its JSON."""
    executable = ROOT / "lift_sim"
    if not executable.exists() and (ROOT / "lift_sim.exe").exists():
        executable = ROOT / "lift_sim.exe"
    if not executable.exists():
        raise RuntimeError("lift_sim is missing; run make first")
    with tempfile.TemporaryDirectory(prefix="lift-web-") as folder:
        temp = Path(folder)
        scenario = temp / "web_demo.csv"
        output = temp / "out"
        scenario.write_text(scenario_text(settings), encoding="utf-8")
        result: dict[str, Any] = {"settings": settings}
        for policy in ("fcfs", "nearest"):
            command = [str(executable), "--scenario", str(scenario), "--policy", policy,
                       "--no-ui", "--tick-ms", "1", "--seed", str(settings["seed"]),
                       "--out", str(output)]
            environment = os.environ.copy()
            if os.name == "nt" and Path("C:/msys64/usr/bin").exists():
                environment["PATH"] = "C:/msys64/usr/bin;" + environment.get("PATH", "")
            run = subprocess.run(command, cwd=ROOT, capture_output=True, text=True,
                                 timeout=25, env=environment)
            if run.returncode:
                detail = (run.stderr or run.stdout).strip().splitlines()
                raise RuntimeError(detail[-1] if detail else f"C simulator exited with {run.returncode}")
            result[policy] = json.loads((output / f"run_{policy}.json").read_text(encoding="utf-8"))
        return result


class Handler(SimpleHTTPRequestHandler):
    """Serve the project UI and one intentionally small JSON endpoint."""

    def __init__(self, *args: Any, **kwargs: Any) -> None:
        super().__init__(*args, directory=str(ROOT), **kwargs)

    def log_message(self, message: str, *args: Any) -> None:
        print(f"[web] {self.address_string()} - {message % args}")

    def do_GET(self) -> None:
        if self.path in ("/", "/index.html"):
            self.path = "/viz/index.html"
        super().do_GET()

    def do_POST(self) -> None:
        if self.path != "/api/simulate":
            self.send_error(HTTPStatus.NOT_FOUND)
            return
        try:
            size = int(self.headers.get("Content-Length", "0"))
            if size < 2 or size > 4096:
                raise ValueError("request body is invalid")
            payload = json.loads(self.rfile.read(size))
            if not isinstance(payload, dict):
                raise ValueError("request must be an object")
            body = json.dumps(simulate(validated(payload)), separators=(",", ":")).encode()
            self.send_response(HTTPStatus.OK)
        except ValueError as error:
            body = json.dumps({"error": str(error)}).encode()
            self.send_response(HTTPStatus.BAD_REQUEST)
        except (RuntimeError, subprocess.TimeoutExpired, OSError, json.JSONDecodeError) as error:
            body = json.dumps({"error": str(error)}).encode()
            self.send_response(HTTPStatus.INTERNAL_SERVER_ERROR)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)


def main() -> None:
    parser = argparse.ArgumentParser(description="Serve the lift simulator web interface")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    server = ThreadingHTTPServer((args.host, args.port), Handler)
    print(f"\nLift simulator ready: http://{args.host}:{args.port}\nPress Ctrl+C to stop.\n", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping web simulator.")
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
