#!/usr/bin/env python3
"""Binary IPC client for Flux `--test-mode` (wire format matches Flux v1)."""

from __future__ import annotations

import json
import os
import socket
import struct
import subprocess
import sys
import time
import unittest
from dataclasses import dataclass
from typing import Any, Iterator, Optional

FLUX_TEST_MAGIC = 0x58554C46
VERSION = 1

# Opcodes (must match TestServer::Op)
GET_UI = 1
GET_SCREENSHOT = 2
CLICK = 3
TYPE = 4
KEY = 5
SCROLL = 6
HOVER = 7
DRAG = 8


def pack_request(op: int, body: bytes = b"") -> bytes:
    return (
        struct.pack("<I", FLUX_TEST_MAGIC)
        + struct.pack("<H", VERSION)
        + struct.pack("<H", op)
        + struct.pack("<I", len(body))
        + body
    )


def read_exact(sock: socket.socket, n: int) -> bytes:
    buf = b""
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("connection closed")
        buf += chunk
    return buf


def read_response(sock: socket.socket) -> tuple[int, int, bytes]:
    hdr = read_exact(sock, 8)
    status = hdr[0]
    payload_type = hdr[1]
    body_len = struct.unpack_from("<I", hdr, 4)[0]
    body = read_exact(sock, body_len) if body_len else b""
    return status, payload_type, body


class FluxTestClient:
    """Each IPC request uses a **new TCP connection** — the server closes the socket after one exchange."""

    def __init__(
        self,
        host: str = "127.0.0.1",
        port: int = 8435,
        *,
        unix_path: Optional[str] = None,
    ) -> None:
        self._host = host
        self._port = port
        self._unix_path = unix_path

    def close(self) -> None:
        pass

    def _call(self, op: int, body: bytes = b"") -> tuple[int, int, bytes]:
        if self._unix_path:
            s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            s.settimeout(60)
            s.connect(self._unix_path)
        else:
            s = socket.create_connection((self._host, self._port), timeout=30)
            s.settimeout(60)
        try:
            s.sendall(pack_request(op, body))
            return read_response(s)
        finally:
            s.close()

    def get_ui(self) -> dict[str, Any]:
        status, ptype, body = self._call(GET_UI)
        if status != 0:
            raise RuntimeError(body.decode("utf-8", errors="replace"))
        if ptype != 0:
            raise RuntimeError("unexpected payload type for GetUi")
        text = body.decode("utf-8")
        if not text.strip():
            return {}
        return json.loads(text)

    def get_screenshot_png(self) -> bytes:
        status, ptype, body = self._call(GET_SCREENSHOT)
        if status != 0:
            try:
                err = json.loads(body.decode("utf-8"))
            except json.JSONDecodeError:
                err = body.decode("utf-8", errors="replace")
            raise RuntimeError(err)
        if ptype != 1:
            raise RuntimeError("expected PNG payload")
        return body

    def click(self, x: float, y: float) -> dict[str, Any]:
        body = json.dumps({"x": x, "y": y}).encode("utf-8")
        status, ptype, data = self._call(CLICK, body)
        if status != 0:
            raise RuntimeError(data.decode("utf-8", errors="replace"))
        return json.loads(data.decode("utf-8"))

    def type_text(self, text: str) -> dict[str, Any]:
        body = json.dumps({"text": text}).encode("utf-8")
        status, ptype, data = self._call(TYPE, body)
        if status != 0:
            raise RuntimeError(data.decode("utf-8", errors="replace"))
        return json.loads(data.decode("utf-8"))

    def key(
        self,
        name: str,
        modifiers: Optional[list[str]] = None,
    ) -> dict[str, Any]:
        d: dict[str, Any] = {"key": name}
        if modifiers:
            d["modifiers"] = modifiers
        body = json.dumps(d).encode("utf-8")
        status, ptype, data = self._call(KEY, body)
        if status != 0:
            raise RuntimeError(data.decode("utf-8", errors="replace"))
        return json.loads(data.decode("utf-8"))

    def scroll(self, x: float, y: float, delta_x: float, delta_y: float) -> dict[str, Any]:
        body = json.dumps(
            {"x": x, "y": y, "deltaX": delta_x, "deltaY": delta_y}
        ).encode("utf-8")
        status, ptype, data = self._call(SCROLL, body)
        if status != 0:
            raise RuntimeError(data.decode("utf-8", errors="replace"))
        return json.loads(data.decode("utf-8"))

    def hover(self, x: float, y: float) -> dict[str, Any]:
        body = json.dumps({"x": x, "y": y}).encode("utf-8")
        status, ptype, data = self._call(HOVER, body)
        if status != 0:
            raise RuntimeError(data.decode("utf-8", errors="replace"))
        return json.loads(data.decode("utf-8"))

    def drag(
        self,
        start_x: float,
        start_y: float,
        end_x: float,
        end_y: float,
        steps: float = 10,
    ) -> dict[str, Any]:
        body = json.dumps(
            {
                "startX": start_x,
                "startY": start_y,
                "endX": end_x,
                "endY": end_y,
                "steps": steps,
            }
        ).encode("utf-8")
        status, ptype, data = self._call(DRAG, body)
        if status != 0:
            raise RuntimeError(data.decode("utf-8", errors="replace"))
        return json.loads(data.decode("utf-8"))


def connect_tcp(host: str, port: int) -> FluxTestClient:
    """Return a client after verifying the test server accepts TCP (handshake connect + close)."""
    s = socket.create_connection((host, port), timeout=30)
    s.close()
    return FluxTestClient(host, port)


def connect_unix(path: str) -> FluxTestClient:
    return FluxTestClient(unix_path=path)


def wait_for_ui_tree(client: FluxTestClient, timeout: float = 22.0) -> dict[str, Any]:
    """Poll GetUi until the root node is populated (first layout has run)."""
    time.sleep(1.0)
    deadline = time.time() + timeout
    while time.time() < deadline:
        tree = client.get_ui()
        if tree.get("type") or tree.get("children"):
            return tree
        time.sleep(0.15)
    return client.get_ui()


def walk_nodes(node: dict[str, Any]) -> Iterator[dict[str, Any]]:
    yield node
    for ch in node.get("children") or []:
        yield from walk_nodes(ch)


def find_node(
    tree: dict[str, Any],
    *,
    focus_key: Optional[str] = None,
    type_substr: Optional[str] = None,
    text: Optional[str] = None,
) -> Optional[dict[str, Any]]:
    for n in walk_nodes(tree):
        if focus_key is not None and n.get("focusKey") != focus_key:
            continue
        if type_substr is not None and type_substr not in str(n.get("type", "")):
            continue
        if text is not None and n.get("text") != text:
            continue
        return n
    return None


def center_of_bounds(node: dict[str, Any]) -> tuple[float, float]:
    b = node["bounds"]
    return (b["x"] + b["w"] * 0.5, b["y"] + b["h"] * 0.5)


def start_test_app(binary_name: str, *, max_attempts: Optional[int] = None) -> FluxAppProcess:
    """Launch ``binary_name`` from ``$FLUX_TEST_BUILD_DIR/tests/ui/`` with a free TCP port.

    Set ``FLUX_TEST_START_ATTEMPTS`` (default ``1``) to retry when the first GUI frame is slow.
    """
    bd = os.environ.get("FLUX_TEST_BUILD_DIR")
    if not bd:
        raise RuntimeError("FLUX_TEST_BUILD_DIR is not set")
    path = os.path.join(bd, "tests", "ui", binary_name)
    if max_attempts is None:
        max_attempts = max(1, int(os.environ.get("FLUX_TEST_START_ATTEMPTS", "2")))
    last: Optional[Exception] = None
    for attempt in range(max_attempts):
        try:
            return FluxAppProcess.start(path, port=free_port())
        except (RuntimeError, OSError) as e:
            last = e
            time.sleep(1.5 + float(attempt))
    assert last is not None
    raise RuntimeError(f"start_test_app failed after {max_attempts} attempts: {last}") from last


def start_test_app_skip(binary_name: str, *, max_attempts: Optional[int] = None) -> FluxAppProcess:
    """Like `start_test_app`, but turns \"empty GetUi\" failures into `unittest.SkipTest` (no GUI session)."""
    try:
        return start_test_app(binary_name, max_attempts=max_attempts)
    except RuntimeError as e:
        if "GetUi stayed empty" in str(e):
            raise unittest.SkipTest(
                "UI tree not ready — run on a machine with an interactive GUI (or increase "
                "FLUX_TEST_START_ATTEMPTS / timing)."
            ) from e
        raise


def free_port() -> int:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(("", 0))
    p = s.getsockname()[1]
    s.close()
    return p


@dataclass
class FluxAppProcess:
    """Runs a Flux test app with `--test-mode` and exposes a connected client."""

    process: subprocess.Popen
    client: FluxTestClient
    port: Optional[int] = None
    socket_path: Optional[str] = None

    def close(self) -> None:
        self.client.close()
        self.process.terminate()
        try:
            self.process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.process.kill()

    @classmethod
    def start(
        cls,
        executable: str,
        *,
        port: Optional[int] = None,
        extra_args: Optional[list[str]] = None,
        env: Optional[dict[str, str]] = None,
    ) -> FluxAppProcess:
        exe_dir = os.path.dirname(os.path.abspath(executable))
        merged_env = os.environ.copy()
        if env:
            merged_env.update(env)
        # Bundled fonts live next to the binary (see CMake POST_BUILD copy).
        merged_env.setdefault("FLUX_FONT_DIRS", os.path.join(exe_dir, "fonts"))

        if port is None:
            port = free_port()
        args = [executable, "--test-mode", "--test-port", str(port)]
        if extra_args:
            args.extend(extra_args)

        proc = subprocess.Popen(
            args,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            env=merged_env,
        )
        connect_deadline = time.time() + 45
        c: Optional[FluxTestClient] = None
        while time.time() < connect_deadline:
            if proc.poll() is not None:
                raise RuntimeError(
                    f"process exited early with code {proc.returncode}"
                )
            try:
                c = connect_tcp("127.0.0.1", port)
                break
            except OSError:
                time.sleep(0.05)
        if c is None:
            proc.terminate()
            raise RuntimeError("timed out waiting for test IPC port")
        # One no-op click wakes input + waits for a present so the first layout/build completes.
        try:
            c.click(4.0, 4.0)
        except Exception:
            pass
        tree = wait_for_ui_tree(c, timeout=22.0)
        if not (tree.get("type") or tree.get("children")):
            proc.terminate()
            raise RuntimeError(
                "GetUi stayed empty — ensure a GUI session and try again "
                "(first Cocoa frame can be slow)."
            )
        return cls(proc, c, port=port)


def main() -> None:
    if len(sys.argv) < 3:
        print("Usage: flux_test_client.py <host> <port> [get_ui|screenshot]")
        sys.exit(1)
    host = sys.argv[1]
    port = int(sys.argv[2])
    cmd = sys.argv[3] if len(sys.argv) > 3 else "get_ui"
    c = connect_tcp(host, port)
    try:
        if cmd == "get_ui":
            print(json.dumps(c.get_ui(), indent=2))
        elif cmd == "screenshot":
            png = c.get_screenshot_png()
            sys.stdout.buffer.write(png)
        else:
            print("unknown command", cmd)
            sys.exit(1)
    finally:
        c.close()


if __name__ == "__main__":
    main()
