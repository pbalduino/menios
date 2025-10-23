#!/bin/sh
set -eu
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
SDK_ROOT="${MENIOS_SDK_ROOT:-$(dirname "$SCRIPT_DIR")/build/sdk}"
if [ ! -d "$SDK_ROOT/include" ]; then
  # fallback to repository root for backward compatibility
  SDK_ROOT="$(dirname "$SCRIPT_DIR")"
fi
CROSS_PREFIX="${MENIOS_CROSS_PREFIX:-x86_64-elf}"

if [ -n "${MENIOS_HOST_CC:-}" ]; then
  CC="${MENIOS_HOST_CC}"
elif command -v "${CROSS_PREFIX}-gcc" >/dev/null 2>&1; then
  CC="${CROSS_PREFIX}-gcc"
else
  CC="gcc"
fi

HOST_BUILD_MODE=0
case "${MENIOS_HOST_BUILD:-}" in
  ""|0|false|FALSE)
    HOST_BUILD_MODE=0
    ;;
  *)
    HOST_BUILD_MODE=1
    ;;
esac

if [ "$HOST_BUILD_MODE" -eq 1 ]; then
  WRAPPER_CFLAGS="${MENIOS_HOST_CFLAGS:-}"
  WRAPPER_LDFLAGS="${MENIOS_HOST_LDFLAGS:-}"
else
  if "$CC" --version 2>/dev/null | head -1 | grep -qi clang; then
    ARCH_FLAGS="-target x86_64-unknown-elf -fuse-ld=lld"
  else
    ARCH_FLAGS=""
  fi

  if [ -n "${MENIOS_ENABLE_SSE:-}" ]; then
    FLOAT_FLAGS="-m64 -mno-red-zone"
  else
    FLOAT_FLAGS="-m64 -mno-red-zone -mno-80387 -mno-mmx -mno-sse -mno-sse2"
  fi

  WRAPPER_CFLAGS="-ffreestanding -fno-stack-protector $FLOAT_FLAGS -nostdlib -nostartfiles -isystem ${SDK_ROOT}/include"
  WRAPPER_CFLAGS="$WRAPPER_CFLAGS $ARCH_FLAGS"
  WRAPPER_LDFLAGS="-nostdlib -nostartfiles ${SDK_ROOT}/lib/crt0.o -L${SDK_ROOT}/lib -lmeniosc -lgcc -static -T ${SDK_ROOT}/lib/user_elf.ld"
fi
if [ "$1" = "-qversion" ]; then
  exec "$CC" --version
fi
COMPILE_ONLY=0
for arg in "$@"; do
  case "$arg" in
    -c|-S|-E)
      COMPILE_ONLY=1
      ;;
  esac
done
if [ "$COMPILE_ONLY" -eq 1 ]; then
  exec "$CC" $WRAPPER_CFLAGS "$@"
else
  exec "$CC" $WRAPPER_CFLAGS "$@" $WRAPPER_LDFLAGS
fi
