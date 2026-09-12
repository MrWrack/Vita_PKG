#!/usr/bin/env bash
set -euo pipefail
mkdir -p resources
curl -L --fail --retry 3 \
  https://raw.githubusercontent.com/TheOfficialFloW/VitaShell/master/resources/head.bin \
  -o resources/head.bin
test -s resources/head.bin
echo "Fetched VitaShell GPL head.bin template."
