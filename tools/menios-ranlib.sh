#!/bin/sh
set -eu

if [ -n "${MENIOS_HOST_RANLIB:-}" ]; then
  RANLIB="${MENIOS_HOST_RANLIB}"
elif command -v x86_64-elf-ranlib >/dev/null 2>&1; then
  RANLIB="x86_64-elf-ranlib"
else
  RANLIB="ranlib"
fi

exec "$RANLIB" "$@"
