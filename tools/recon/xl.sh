#!/bin/sh
# xl.sh ARGS...  - kill any xldb on the guest, then start "xldb ARGS" in ~ on :45.
# Env: PRE='shell commands' run first on the guest (in ~).
here=$(dirname "$0")
timeout 90 python3 "$here/aix.py" \
  "for p in \$(ps -ef | grep -w xldb | grep -v grep | awk '{print \$2}'); do kill \$p; done; sleep 1; cd ~; rm -f ~/.xldb.rich; ${PRE:-true}" \
  "cd ~; DISPLAY=192.168.76.1:45 nohup xldb $* > /tmp/xl.out 2>&1 &" \
  "sleep 10; ps -ef | grep -v grep | grep -w -e xldb -e rich -e sleep"
