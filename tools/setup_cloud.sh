#!/bin/bash
# Bootstrap the cambc engine + toolchains in a fresh cloud sandbox (Claude Code on the web).
# The engine's built-in downloader fails behind the sandbox TLS proxy ("invalid peer certificate"), so the
# toolchains are fetched with curl (which trusts the proxy CA) and placed where cambc expects them.
# usage: bash tools/setup_cloud.sh      (idempotent; ~300 MB download on first run)
set -e
cd "$(dirname "$0")/.."
if [ ! -x .venv/bin/cambc ]; then
  python3 -m venv .venv
  .venv/bin/pip install -q cambc==2.1.4
fi
C=~/.cache/cambc
mkdir -p "$C"
tmp=$(mktemp -d)
if [ ! -x "$C/wasi-sdk-33/bin/clang++" ]; then
  curl -sSL -o "$tmp/wasi.tgz" https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-33/wasi-sdk-33.0-x86_64-linux.tar.gz
  mkdir -p "$C/wasi-sdk-33" && tar xzf "$tmp/wasi.tgz" -C "$C/wasi-sdk-33" --strip-components=1
fi
if [ ! -x "$C/cmake-3.31.8-linux-x86_64/bin/cmake" ]; then
  curl -sSL -o "$tmp/cmake.tgz" https://github.com/Kitware/CMake/releases/download/v3.31.8/cmake-3.31.8-linux-x86_64.tar.gz
  echo "630615d8e98ac33eba7fbe472626dff5c899c85af3c024585ae109166a6909d0  $tmp/cmake.tgz" | sha256sum -c -
  mkdir -p "$C/cmake-3.31.8-linux-x86_64" && tar xzf "$tmp/cmake.tgz" -C "$C/cmake-3.31.8-linux-x86_64" --strip-components=1
fi
if [ ! -x "$C/ninja-1.12.1-linux-x86_64/ninja" ]; then
  curl -sSL -o "$tmp/ninja.zip" https://github.com/ninja-build/ninja/releases/download/v1.12.1/ninja-linux.zip
  echo "6f98805688d19672bd699fbbfa2c2cf0fc054ac3df1f0e6a47664d963d530255  $tmp/ninja.zip" | sha256sum -c -
  mkdir -p "$C/ninja-1.12.1-linux-x86_64" && unzip -oq "$tmp/ninja.zip" -d "$C/ninja-1.12.1-linux-x86_64" && chmod +x "$C/ninja-1.12.1-linux-x86_64/ninja"
fi
rm -rf "$tmp"
.venv/bin/cambc build beast && echo "cambc ready: try  python3 tools/match.py beast beast_v13 --jobs 4"
# The replay visualiser assets (needed by tools/replay_dump.mjs) are cached on the first `cambc view` call:
#   timeout 10 .venv/bin/cambc view ladder_replays/whim_lanes.zip --port 7099 || true
