#!/bin/sh
set -eu

if [ -n "${MENIOS_HOST_AR:-}" ]; then
  AR="${MENIOS_HOST_AR}"
elif command -v x86_64-elf-ar >/dev/null 2>&1; then
  AR="x86_64-elf-ar"
else
  AR="ar"
fi

exec "$AR" "$@"
