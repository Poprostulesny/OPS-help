#!/usr/bin/env python3
import argparse
import socket
import struct
import sys
import time


BOARD_SIZE = 8
SPELLS = {
    0: "Divination",
    1: "Summon Elemental",
    2: "Fireball",
}


class Client:
    def __init__(self, label, host, port, timeout, dry_run):
        self.label = label
        self.addr = (host, port)
        self.sock = None
        if not dry_run:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.sock.bind(("127.0.0.1", 0))
            self.sock.settimeout(timeout)

    @property
    def local(self):
        if self.sock is None:
            return "dry-run"
        host, port = self.sock.getsockname()
        return f"{host}:{port}"

    def send(self, packet, note, dry_run):
        print(f"{self.label:<9} -> {note:<42} {packet.hex(' ')}")
        if not dry_run:
            self.sock.sendto(packet, self.addr)

    def listen(self, seconds, endian):
        if self.sock is None:
            return
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            try:
                data = self.sock.recv(4096)
            except socket.timeout:
                continue
            print(f"{self.label:<9} <- {describe_reply(data, endian):<42} {data.hex(' ')}")

    def close(self):
        if self.sock is not None:
            self.sock.close()


def login(name):
    raw = name.encode("utf-8")[:14]
    return b"l\0" + raw.ljust(14, b"\0")


def quit_msg():
    return b"q\0" + (b"\0" * 14)


def cast(spell, x, y, endian):
    fmt = "<HHH" if endian == "little" else "!HHH"
    return b"c\0" + struct.pack(fmt, spell, x, y).ljust(14, b"\0")


def bad_size():
    return b"x"


def bad_type():
    return b"z\0" + (b"\0" * 14)


def describe_reply(data, endian):
    if len(data) == 1:
        if data == b"w":
            return "winner marker 'w'"
        if data == b"l":
            return "loser marker 'l'"
        return f"1-byte reply {data!r}"
    if len(data) == 2:
        value = unpack_u16(data, endian)
        return f"pouch update: {value}"
    if len(data) == 50:
        values = [unpack_u16(data[i:i + 2], endian) for i in range(0, 50, 2)]
        return f"divination 5x5: {values}"
    return f"{len(data)}-byte reply"


def unpack_u16(data, endian):
    fmt = "<H" if endian == "little" else "!H"
    return struct.unpack(fmt, data)[0]


def pause(args):
    if args.delay > 0:
        time.sleep(args.delay)


def send(client, packet, note, args):
    client.send(packet, note, args.dry_run)
    pause(args)


def scenario_stage1(clients, args):
    a = clients["A"]
    send(a, login("Merlin"), "login Merlin", args)
    send(a, cast(2, 3, 4, args.endian), "cast Fireball 3,4", args)
    send(a, quit_msg(), "quit", args)
    send(a, login("Eleonora"), "login Eleonora (4th message)", args)


def scenario_validation(clients, args):
    a = clients["A"]
    send(a, bad_size(), "bad size: 1 byte", args)
    send(a, bad_type(), "bad type: 'z'", args)
    send(a, cast(99, 1, 1, args.endian), "bad spell: 99", args)
    send(a, cast(1, BOARD_SIZE, 0, args.endian), "bad coord: x == BOARD_SIZE", args)
    send(a, login("Valid"), "valid login after errors", args)


def scenario_stage2(clients, args):
    a = clients["A"]
    b = clients["B"]
    send(a, login("Archibald"), "login player A", args)
    send(b, login("Eleonora"), "login player B", args)
    for i in range(args.count):
        spell = i % 3
        x = i % BOARD_SIZE
        y = (i + 1) % BOARD_SIZE
        send(a, cast(spell, x, y, args.endian), f"queue cast {SPELLS[spell]} {x},{y}", args)


def scenario_stage3(clients, args):
    a = clients["A"]
    b = clients["B"]
    x = clients["X"]
    send(a, cast(0, 1, 1, args.endian), "cast before login, should reject", args)
    send(a, login("Archibald"), "login player A", args)
    send(b, login("Eleonora"), "login player B", args)
    send(x, cast(2, 2, 2, args.endian), "outsider cast, should reject", args)
    send(a, cast(0, 1, 1, args.endian), "player A Divination 1,1", args)
    send(b, quit_msg(), "player B surrender", args)


def scenario_stage4(clients, args):
    a = clients["A"]
    b = clients["B"]
    send(a, login("Archibald"), "login player A", args)
    send(b, login("Eleonora"), "login player B", args)
    send(a, cast(1, 2, 2, args.endian), "A Summon Elemental 2,2", args)
    time.sleep(max(args.delay, 0.25))
    send(b, cast(0, 2, 2, args.endian), "B Divination 2,2", args)
    send(a, cast(2, 2, 2, args.endian), "A Fireball 2,2", args)


def scenario_custom(clients, args):
    a = clients["A"]
    if args.message == "login":
        send(a, login(args.name), f"login {args.name}", args)
    elif args.message == "cast":
        send(a, cast(args.spell, args.x, args.y, args.endian), f"cast {args.spell} {args.x},{args.y}", args)
    elif args.message == "quit":
        send(a, quit_msg(), "quit", args)
    elif args.message == "bad-size":
        send(a, bad_size(), "bad size", args)
    elif args.message == "bad-type":
        send(a, bad_type(), "bad type", args)
    else:
        raise ValueError(f"unknown custom message: {args.message}")


SCENARIOS = {
    "stage1": scenario_stage1,
    "validation": scenario_validation,
    "stage2": scenario_stage2,
    "stage3": scenario_stage3,
    "stage4": scenario_stage4,
    "custom": scenario_custom,
}


def make_clients(args):
    clients = {
        "A": Client("player A", args.host, args.port, args.timeout, args.dry_run),
        "B": Client("player B", args.host, args.port, args.timeout, args.dry_run),
        "X": Client("outsider", args.host, args.port, args.timeout, args.dry_run),
    }
    print(f"target    {args.host}:{args.port}")
    for client in clients.values():
        print(f"{client.label:<9} local {client.local}")
    print()
    return clients


def main():
    parser = argparse.ArgumentParser(description="UDP data generator for L8 Mages' Chess.")
    parser.add_argument("port", type=int, help="server UDP port")
    parser.add_argument("--host", default="127.0.0.1", help="server host")
    parser.add_argument("--stage", choices=SCENARIOS.keys(), default="stage1", help="data scenario to send")
    parser.add_argument("--endian", choices=["big", "little"], default="big", help="uint16 byte order")
    parser.add_argument("--delay", type=float, default=0.30, help="delay after each sent datagram")
    parser.add_argument("--count", type=int, default=14, help="number of casts in stage2")
    parser.add_argument("--listen", type=float, default=0.0, help="listen for UDP replies after sending, in seconds")
    parser.add_argument("--dry-run", action="store_true", help="print packets without sending them")
    parser.add_argument("--timeout", type=float, default=0.25, help="socket receive timeout while listening")
    parser.add_argument("--message", choices=["login", "cast", "quit", "bad-size", "bad-type"], default="login")
    parser.add_argument("--name", default="Merlin", help="custom login name")
    parser.add_argument("--spell", type=int, default=0, help="custom spell id")
    parser.add_argument("--x", type=int, default=0, help="custom target x")
    parser.add_argument("--y", type=int, default=0, help="custom target y")
    args = parser.parse_args()

    clients = make_clients(args)
    try:
        SCENARIOS[args.stage](clients, args)
        if args.listen > 0 and not args.dry_run:
            print()
            print(f"listening for replies for {args.listen:.2f}s")
            for client in clients.values():
                client.listen(args.listen, args.endian)
    finally:
        for client in clients.values():
            client.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
