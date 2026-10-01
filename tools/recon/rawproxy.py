#!/usr/bin/env python3
"""rawproxy.py LISTEN_PORT TARGET_HOST TARGET_PORT OUTDIR

Minimal TCP proxy for X11 connections.  Forwards both directions and logs
each connection's raw client->server bytes to OUTDIR/conn-N.c2s and
server->client bytes to OUTDIR/conn-N.s2c, so requests such as PutImage
can be decoded exactly (xtrace does not print image data).
"""
import os
import select
import socket
import sys
import threading

listen_port, host, port, outdir = int(sys.argv[1]), sys.argv[2], int(sys.argv[3]), sys.argv[4]
os.makedirs(outdir, exist_ok=True)
counter = 0
lock = threading.Lock()


def pump(client, n):
    server = socket.create_connection((host, port))
    logs = {client: open(f"{outdir}/conn-{n}.c2s", "wb"),
            server: open(f"{outdir}/conn-{n}.s2c", "wb")}
    peer = {client: server, server: client}
    socks = [client, server]
    try:
        while True:
            r, _, _ = select.select(socks, [], [])
            for s in r:
                data = s.recv(65536)
                if not data:
                    return
                logs[s].write(data)
                logs[s].flush()
                peer[s].sendall(data)
    finally:
        for f in logs.values():
            f.close()
        client.close()
        server.close()


srv = socket.socket()
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("0.0.0.0", listen_port))
srv.listen(8)
while True:
    c, _ = srv.accept()
    with lock:
        counter += 1
        n = counter
    threading.Thread(target=pump, args=(c, n), daemon=True).start()
