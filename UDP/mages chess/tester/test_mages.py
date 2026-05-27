#!/usr/bin/env python3
import argparse
import errno
import fcntl
import os
import pty
import select
import socket
import struct
import subprocess
import sys
import time
from pathlib import Path


TESTER_ROOT = Path(__file__).resolve().parent
ROOT = TESTER_ROOT.parent
DEFAULT_BINARY = ROOT / "sop-mag"
MSG_SIZE = 16
BOARD_SIZE = 8


class TestFailure(Exception):
    pass


class Client:
    def __init__(self, server_addr, timeout=1.0):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("127.0.0.1", 0))
        self.sock.settimeout(timeout)
        self.server_addr = server_addr

    @property
    def port(self):
        return self.sock.getsockname()[1]

    def send(self, payload):
        self.sock.sendto(payload, self.server_addr)

    def recv(self, size=4096, timeout=1.0):
        old_timeout = self.sock.gettimeout()
        self.sock.settimeout(timeout)
        try:
            return self.sock.recv(size)
        finally:
            self.sock.settimeout(old_timeout)

    def drain(self, duration=0.2):
        end = time.monotonic() + duration
        packets = []
        self.sock.setblocking(False)
        try:
            while time.monotonic() < end:
                try:
                    packets.append(self.sock.recv(4096))
                except BlockingIOError:
                    time.sleep(0.01)
        finally:
            self.sock.setblocking(True)
        return packets

    def close(self):
        self.sock.close()


class Server:
    def __init__(self, binary, port, extra_args=None):
        self.port = port
        args = [str(binary), str(port)]
        if extra_args:
            args.extend(extra_args)
        env = os.environ.copy()
        env.setdefault("ASAN_OPTIONS", "detect_leaks=0:abort_on_error=1")
        env.setdefault("LSAN_OPTIONS", "detect_leaks=0")
        master_fd, slave_fd = pty.openpty()
        self.master_fd = master_fd
        flags = fcntl.fcntl(self.master_fd, fcntl.F_GETFL)
        fcntl.fcntl(self.master_fd, fcntl.F_SETFL, flags | os.O_NONBLOCK)
        self.proc = subprocess.Popen(
            args,
            cwd=ROOT,
            stdout=slave_fd,
            stderr=slave_fd,
            close_fds=True,
            env=env,
        )
        os.close(slave_fd)
        time.sleep(0.15)
        if self.proc.poll() is not None:
            raise TestFailure(f"server exited immediately:\n{self.output(0.2)}")

    def output(self, duration=0.0):
        end = time.monotonic() + duration
        chunks = []
        while True:
            timeout = max(0.0, end - time.monotonic()) if duration else 0
            readable, _, _ = select.select([self.master_fd], [], [], timeout)
            if not readable:
                break
            try:
                data = os.read(self.master_fd, 4096)
            except OSError as exc:
                if exc.errno in (errno.EIO, errno.EBADF):
                    break
                raise
            if data:
                chunks.append(data.decode("utf-8", errors="replace"))
            if duration and time.monotonic() >= end:
                break
        return "".join(chunks)

    def wait(self, timeout=3.0):
        try:
            self.proc.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            return None
        return self.proc.returncode

    def terminate(self):
        if self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=1.0)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait(timeout=1.0)
        out = self.output(0.2)
        try:
            os.close(self.master_fd)
        except OSError:
            pass
        return out


def free_port():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("127.0.0.1", 0))
    port = sock.getsockname()[1]
    sock.close()
    return port


def login(name):
    raw = name.encode("utf-8")[:14]
    return b"l\0" + raw.ljust(14, b"\0")


def quit_msg():
    return b"q\0" + (b"\0" * 14)


def cast(spell, x, y, endian):
    fmt = "<HHH" if endian == "little" else "!HHH"
    body = struct.pack(fmt, spell, x, y).ljust(14, b"\0")
    return b"c\0" + body


def parse_u16(data, endian):
    fmt = "<H" if endian == "little" else "!H"
    return struct.unpack(fmt, data[:2])[0]


def assert_contains(text, needle, label):
    if needle not in text:
        raise TestFailure(f"{label}: missing line containing {needle!r}\n--- output ---\n{text}")


def assert_any_contains(text, needles, label):
    if not any(needle in text for needle in needles):
        raise TestFailure(f"{label}: missing any of {needles!r}\n--- output ---\n{text}")


def build(binary):
    result = subprocess.run(["make"], cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode != 0:
        raise TestFailure(f"make failed:\n{result.stdout}")
    if not binary.exists():
        raise TestFailure(f"make succeeded, but {binary.name} was not created")


def scenario_stage1(binary, endian):
    srv = Server(binary, free_port())
    client = Client(("127.0.0.1", srv.port))
    try:
        client.send(login("Merlin"))
        client.send(cast(2, 3, 4, endian))
        client.send(quit_msg())
        client.send(login("Eleonora"))
        code = srv.wait(3.0)
        out = srv.output(0.2)
        if code is None:
            raise TestFailure("stage1: server did not exit after 4 valid messages")
        assert_contains(out, "[Login] Welcome, Merlin", "stage1")
        assert_any_contains(out, ["[Cast] Someone casts Fireball onto 3,4", "[Cast] Merlin casts Fireball onto 3,4"], "stage1")
        assert_any_contains(out, ["[Quit] Someone quit. Goodbye!", "[Quit] Merlin quit. Goodbye!"], "stage1")
    finally:
        out = srv.terminate()
        client.close()
    return out


def scenario_validation(binary, endian):
    srv = Server(binary, free_port())
    client = Client(("127.0.0.1", srv.port))
    try:
        client.send(b"x")
        client.send(b"z\0" + b"\0" * 14)
        client.send(cast(99, 1, 1, endian))
        client.send(cast(1, BOARD_SIZE, 0, endian))
        client.send(login("Valid"))
        time.sleep(0.4)
        out = srv.output(0.2)
        if srv.proc.poll() is not None:
            raise TestFailure(f"validation: server crashed or exited after invalid input\n--- output ---\n{out}")
        assert_contains(out, "[Login] Welcome, Valid", "validation")
    finally:
        out = srv.terminate()
        client.close()
    return out


def scenario_stage2(binary, endian):
    srv = Server(binary, free_port())
    addr = ("127.0.0.1", srv.port)
    a = Client(addr)
    b = Client(addr)
    try:
        a.send(login("Archibald"))
        b.send(login("Eleonora"))
        for i in range(6):
            a.send(cast(i % 3, i % BOARD_SIZE, (i + 1) % BOARD_SIZE, endian))
        time.sleep(1.2)
        out = srv.output(0.3)
        assert_any_contains(out, ["casts Divination", "casts Summon Elemental", "casts Fireball"], "stage2")
    finally:
        out = srv.terminate()
        a.close()
        b.close()
    return out


def scenario_stage3(binary, endian):
    srv = Server(binary, free_port())
    addr = ("127.0.0.1", srv.port)
    a = Client(addr)
    b = Client(addr)
    outsider = Client(addr)
    try:
        a.send(cast(0, 1, 1, endian))
        a.send(login("Archibald"))
        b.send(login("Eleonora"))
        outsider.send(cast(2, 2, 2, endian))
        a.send(cast(0, 1, 1, endian))
        b.send(quit_msg())
        code = srv.wait(3.0)
        out = srv.output(0.3)
        if code is None:
            raise TestFailure("stage3: server did not exit after logged-in player quit")
        assert_contains(out, "[Login] Welcome, Archibald", "stage3")
        assert_contains(out, "[Login] Welcome, Eleonora", "stage3")
        assert_contains(out, "[Quit] Eleonora quit. Goodbye!", "stage3")
        assert_contains(out, "-= Congratulations, Archibald, you win! =-", "stage3")
        if "Someone casts Fireball onto 2,2" in out or "casts Fireball onto 2,2" in out:
            raise TestFailure(f"stage3: server accepted cast from non-player\n--- output ---\n{out}")
    finally:
        out = srv.terminate()
        a.close()
        b.close()
        outsider.close()
    return out


def scenario_stage4(binary, endian):
    srv = Server(binary, free_port())
    addr = ("127.0.0.1", srv.port)
    a = Client(addr, timeout=2.0)
    b = Client(addr, timeout=2.0)
    try:
        a.send(login("Archibald"))
        b.send(login("Eleonora"))
        a.send(cast(1, 2, 2, endian))
        time.sleep(0.4)
        a.drain()
        b.drain()
        b.send(cast(0, 2, 2, endian))
        packets = []
        deadline = time.monotonic() + 2.5
        while time.monotonic() < deadline and not any(len(p) == 50 for p in packets):
            try:
                packets.append(b.recv(timeout=0.25))
            except socket.timeout:
                pass
        divination = next((p for p in packets if len(p) == 50), None)
        out = srv.output(0.3)
        if divination is None:
            raise TestFailure(f"stage4: player did not receive a 50-byte divination response\n--- output ---\n{out}")
        values = [parse_u16(divination[i:i + 2], endian) for i in range(0, 50, 2)]
        center = values[12]
        if center != 2:
            raise TestFailure(f"stage4: expected opponent elemental in divination center, got {center}; values={values}")
        a.send(quit_msg())
        srv.wait(2.0)
        out += srv.output(0.3)
        assert_any_contains(out, ["Archibald", "Eleonora"], "stage4")
    finally:
        out = srv.terminate()
        a.close()
        b.close()
    return out


SCENARIOS = {
    "stage1": scenario_stage1,
    "validation": scenario_validation,
    "stage2": scenario_stage2,
    "stage3": scenario_stage3,
    "stage4": scenario_stage4,
}


def main():
    parser = argparse.ArgumentParser(description="Black-box UDP tests for L8 Mages' Chess.")
    parser.add_argument("--binary", type=Path, default=DEFAULT_BINARY, help="path to server binary")
    parser.add_argument("--stage", choices=["all", *SCENARIOS.keys()], default="all", help="scenario to run")
    parser.add_argument("--endian", choices=["big", "little"], default="big", help="uint16 byte order in UDP packets")
    parser.add_argument("--no-build", action="store_true", help="do not run make before tests")
    parser.add_argument("--verbose", action="store_true", help="print captured server output for passed tests")
    args = parser.parse_args()

    binary = args.binary.resolve()
    if not args.no_build:
        build(binary)
    elif not binary.exists():
        raise TestFailure(f"binary does not exist: {binary}")

    names = list(SCENARIOS) if args.stage == "all" else [args.stage]
    failed = 0
    for name in names:
        try:
            out = SCENARIOS[name](binary, args.endian)
            print(f"[PASS] {name}")
            if args.verbose and out:
                print(out.rstrip())
        except TestFailure as exc:
            failed += 1
            print(f"[FAIL] {name}: {exc}", file=sys.stderr)
        except Exception as exc:
            failed += 1
            print(f"[ERROR] {name}: {exc}", file=sys.stderr)
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
