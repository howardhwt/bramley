#!/usr/bin/env python3
"""A stand-in for TortOS's Muse daemon, for testing the launcher's music screens on a Mac.

It listens on /tmp/muse.sock and speaks Muse's line protocol (verb, then tab-separated
key=value pairs), as the real daemon did in spikes/muse. It plays nothing: it pretends, sending
POS once a second and END when a track runs out. Lengths and tags come from ffprobe if it is
installed. Run it before the desktop launcher:  python3 tools/fake-muse.py
"""
import json
import os
import socket
import subprocess
import threading
import time

SOCK = "/tmp/muse.sock"
clients = []
lock = threading.Lock()
state = {"state": "stopped", "path": "", "at": 0.0, "len": 0.0, "speed": 1.0, "started": 0.0, "meta": {}}


def probe(path):
    try:
        out = subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration:format_tags=title,artist,album",
                              "-of", "json", path], capture_output=True, text=True, timeout=5).stdout
        fmt = json.loads(out).get("format", {})
        tags = {k.lower(): v for k, v in fmt.get("tags", {}).items()}
        return float(fmt.get("duration", 0) or 0), tags
    except Exception:
        return 0.0, {}


def send_all(line):
    with lock:
        for c in list(clients):
            try:
                c.sendall((line + "\n").encode())
            except OSError:
                clients.remove(c)


def position():
    if state["state"] == "playing":
        return min(state["len"], state["at"] + (time.time() - state["started"]) * state["speed"])
    return state["at"]


def send_state():
    send_all(f"STATE\tstate={state['state']}\tpath={state['path']}")


def send_meta():
    m = state["meta"]
    send_all(f"META\ttitle={m.get('title', '')}\tartist={m.get('artist', '')}\talbum={m.get('album', '')}"
             f"\tlen={state['len']:.1f}\tchapters=0")


def handle(line):
    verb, *pairs = line.split("\t")
    args = dict(p.split("=", 1) for p in pairs if "=" in p)
    if verb == "PLAY":
        path = args.get("path", "")
        if not os.path.exists(path):
            send_all("ERROR\twhy=no such file")
            return
        state["len"], state["meta"] = probe(path)
        if not state["len"]:
            state["len"] = 180.0
        state.update(path=path, at=float(args.get("at", 0) or 0), state="playing", started=time.time(),
                     speed=float(args.get("speed", 1) or 1))
        send_meta()
        send_state()
    elif verb == "PAUSE" and state["state"] == "playing":
        state.update(at=position(), state="paused")
        send_state()
    elif verb == "RESUME" and state["state"] == "paused":
        state.update(state="playing", started=time.time())
        send_state()
    elif verb == "STOP":
        state.update(at=position(), state="stopped")
        send_state()
    elif verb == "SEEK":
        state.update(at=max(0.0, min(state["len"], float(args.get("at", 0)))), started=time.time())
        send_all(f"POS\tat={position():.1f}\tlen={state['len']:.1f}")
    elif verb == "SPEED":
        state.update(at=position(), started=time.time(), speed=float(args.get("x", 1)))
    elif verb == "STATUS":
        send_state()
        send_all(f"POS\tat={position():.1f}\tlen={state['len']:.1f}")
        send_all("SINK\tdevice=default")
    elif verb == "COVER":
        send_all(f"COVER\tbase={args.get('base', '')}\tfile=")
    elif verb == "ARTIST":
        send_all(f"ARTIST\tdir={args.get('dir', '')}\tname=")


def ticker():
    while True:
        time.sleep(1)
        if state["state"] == "playing":
            at = position()
            if at >= state["len"]:
                state.update(at=state["len"], state="stopped")
                send_all(f"END\tpath={state['path']}")
                send_state()
            else:
                send_all(f"POS\tat={at:.1f}\tlen={state['len']:.1f}")


def serve(conn):
    with lock:
        clients.append(conn)
    conn.sendall(b"READY\tproto=3\n")
    send_state()
    buf = b""
    while True:
        try:
            data = conn.recv(4096)
        except OSError:
            break
        if not data:
            break
        buf += data
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            if line.strip():
                handle(line.decode(errors="replace").strip())
    with lock:
        if conn in clients:
            clients.remove(conn)


def main():
    if os.path.exists(SOCK):
        os.unlink(SOCK)
    server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    server.bind(SOCK)
    server.listen(4)
    print(f"fake muse: listening on {SOCK}", flush=True)
    threading.Thread(target=ticker, daemon=True).start()
    while True:
        conn, _ = server.accept()
        threading.Thread(target=serve, args=(conn,), daemon=True).start()


if __name__ == "__main__":
    main()
