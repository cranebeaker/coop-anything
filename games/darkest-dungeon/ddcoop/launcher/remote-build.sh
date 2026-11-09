#!/bin/sh
# Sends launcher/ to the MinGW container (CT116 via the Proxmox host "pve"), builds DDCoop.exe there and
# copies it back to build/win/DDCoop.exe. Usage: launcher/remote-build.sh [version]
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/win
TMP="${CLAUDE_JOB_DIR:-/tmp}/ddc-src.tgz"
tar czf "$TMP" launcher
ssh pve "cat > /tmp/ddc-src.tgz && pct push 116 /tmp/ddc-src.tgz /work/ddc-src.tgz && rm /tmp/ddc-src.tgz && \
  pct exec 116 -- bash -c 'rm -rf /work/ddc && mkdir -p /work/ddc && cd /work/ddc && tar xzf ../ddc-src.tgz && \
  DDC_VERSION=${1:-1.0.0} sh launcher/build-windows.sh'" < "$TMP"
ssh pve "pct pull 116 /work/ddc/build/win/DDCoop.exe /tmp/DDCoop.exe && cat /tmp/DDCoop.exe && rm /tmp/DDCoop.exe" \
    > build/win/DDCoop.exe
ssh pve "pct pull 116 /work/ddc/build/win/DDCoop.debug.exe /tmp/DDCoop.debug.exe && cat /tmp/DDCoop.debug.exe && rm /tmp/DDCoop.debug.exe" \
    > build/win/DDCoop.debug.exe
rm -f "$TMP"
ls -l build/win/DDCoop.exe
