#!/bin/sh
set -eu
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
SDK_ROOT="$(dirname "$SCRIPT_DIR")"
CROSS_PREFIX="${MENIOS_CROSS_PREFIX:-x86_64-elf}"

if [ -n "${MENIOS_HOST_CC:-}" ]; then
  CC="${MENIOS_HOST_CC}"
elif command -v "${CROSS_PREFIX}-gcc" >/dev/null 2>&1; then
  CC="${CROSS_PREFIX}-gcc"
else
  CC="gcc"
fi
CFLAGS="-ffreestanding -fno-stack-protector -m64 -mno-red-zone -mno-80387 -mno-mmx -mno-sse -mno-sse2 -nostdlib -nostartfiles -isystem ${SDK_ROOT}/include"
LDFLAGS="-nostdlib -nostartfiles ${SDK_ROOT}/lib/crt0.o -L${SDK_ROOT}/lib -lmeniosc -lgcc -static -T ${SDK_ROOT}/lib/user_elf.ld"
COMPILE_ONLY=0
for arg in "$@"; do
  case "$arg" in
    -c|-S|-E)
      COMPILE_ONLY=1
      ;;
  esac
done
if [ "$COMPILE_ONLY" -eq 1 ]; then
  exec "$CC" $CFLAGS "$@"
else
  exec "$CC" $CFLAGS "$@" $LDFLAGS
fi
