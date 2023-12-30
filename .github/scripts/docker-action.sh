#!/bin/bash
set -ue

cirun() {
  runuser -u qemuci -- "$@"
}

apt-get -y update
DEBIAN_FRONTEND=noninteractive apt-get -y install build-essential cmake curl gettext ninja-build locales-all cpanminus attr libattr1-dev
useradd --create-home qemuci
# Docker runs the commands as root, but we want the build/test to run as
# non-root so permissions based tests run correctly
chown -R qemuci: /github/workspace
cd /github/workspace
cirun cmake -S cmake.deps -B .deps -G Ninja -D USE_BUNDLED_LUAJIT=OFF -D USE_BUNDLED_LUA=ON
cirun cmake --build .deps
cirun cmake -B build -G Ninja -D PREFER_LUA=ON
cirun make ${TARGET}
