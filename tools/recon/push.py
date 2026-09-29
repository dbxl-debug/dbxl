#!/usr/bin/env python3
"""push.py LOCAL REMOTE... -- copy text files to the AIX guest via telnet heredoc."""
import sys
exec(open(__file__.replace('push.py', 'aix.py')).read().split('if __name__')[0])
t = session()
args = sys.argv[1:]
for local, remote in zip(args[0::2], args[1::2]):
    data = open(local).read()
    assert 'XEOFX' not in data
    t.sendline(f"cat > {remote} <<'XEOFX'")
    for line in data.splitlines():
        t.sendline(line)
    t.sendline('XEOFX')
    t.expect(PROMPT)
    print(run(t, f'cksum {remote}'), end='')
t.sendline('exit')
