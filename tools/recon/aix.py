#!/usr/bin/env python3
"""Run shell commands on the AIX guest over telnet.

usage: aix.py 'cmd1' ['cmd2' ...]
Each command's output is printed. Commands run in one ksh session.

The guest and login come from AIX_HOST, AIX_USER and AIX_PASSWORD, by
default the build-emulated-aix guest (192.168.76.2) and its recon account
tester/tester (docs/RECON.md).
"""
import os
import sys
import pexpect

HOST = os.environ.get("AIX_HOST", "192.168.76.2")
USER = os.environ.get("AIX_USER", "tester")
PASSWORD = os.environ.get("AIX_PASSWORD", "tester")
PROMPT = "AIXPROMPT> "


def session(timeout=60):
    t = pexpect.spawn(f"telnet {HOST}", encoding="latin-1", timeout=timeout)
    t.expect("login:")
    t.sendline(USER)
    t.expect("assword:")
    t.sendline(PASSWORD)
    # AIX may ask about TERM or show motd; wait for any shell prompt char
    t.expect([r"\$ ", r"# ", r"> "])
    t.sendline("PS1='AIX''PROMPT> '; export PS1; stty -echo; TERM=dumb; export TERM")
    t.expect(PROMPT)
    return t


def run(t, cmd, timeout=60):
    t.sendline(cmd)
    t.expect(PROMPT, timeout=timeout)
    out = t.before.replace("\r\n", "\n").replace("\r", "")
    return out


if __name__ == "__main__":
    t = session()
    for c in sys.argv[1:]:
        print(f"$ {c}")
        print(run(t, c, timeout=600), end="")
    t.sendline("exit")
