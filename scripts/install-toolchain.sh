#!/usr/bin/env bash
# Install x86_64-elf cross-compiler (binutils + gcc) for BonfireOS.
# Usage: ./scripts/install-toolchain.sh [install_prefix]
# Default prefix: $HOME/.local  (add $HOME/.local/bin to PATH)
# Requires: build-essential, bison, flex, libgmp-dev, libmpc-dev, libmpfr-dev, texinfo, nasm

set -e
TARGET=x86_64-elf
PREFIX="$HOME/.local"
INSTALL_DEPS=1
for arg in "$@"; do
  if [[ "$arg" == "--no-deps" ]]; then INSTALL_DEPS=0
  elif [[ "$arg" != "" && "$arg" != --* ]]; then PREFIX="$arg"; fi
done
BUILD_DIR="${BUILD_DIR:-$HOME/.cache/bonfireos-toolchain-build}"
BINUTILS_VERSION="${BINUTILS_VERSION:-2.42}"
GCC_VERSION="${GCC_VERSION:-13.2.0}"
GNU_MIRROR="${GNU_MIRROR:-https://ftp.gnu.org/gnu}"

echo "Target: $TARGET"
echo "Install prefix: $PREFIX"
echo "Build dir: $BUILD_DIR"
echo "Binutils: $BINUTILS_VERSION  GCC: $GCC_VERSION"
echo ""

# Quick check for required build tools (needed for GCC)
for cmd in bison flex makeinfo; do
  if ! command -v $cmd &>/dev/null; then
    echo "Missing: $cmd. Install with: sudo apt install -y bison flex texinfo"
    echo "Then run this script again (use --no-deps to skip apt step)."
    exit 1
  fi
done

# 1) Install dependencies (Debian/Ubuntu) — skip if --no-deps was passed
INSTALL_DEPS=1
for arg in "$@"; do [[ "$arg" == "--no-deps" ]] && INSTALL_DEPS=0; done
if [[ "$INSTALL_DEPS" -eq 1 ]] && command -v apt-get &>/dev/null; then
  echo "Installing build dependencies (sudo required)..."
  if sudo apt-get update -qq && sudo apt-get install -y --no-install-recommends \
    build-essential bison flex libgmp-dev libmpc-dev libmpfr-dev texinfo \
    nasm git curl; then
    echo "Dependencies OK."
  else
    echo "Dependency install failed. Install manually, then run: $0 --no-deps [prefix]"
    exit 1
  fi
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
export PATH="$PREFIX/bin:$PATH"

# 2) Binutils
if ! command -v "$TARGET-as" &>/dev/null; then
  echo "Building binutils $BINUTILS_VERSION..."
  if [[ ! -f "binutils-$BINUTILS_VERSION.tar.xz" ]]; then
    curl -sL "$GNU_MIRROR/binutils/binutils-$BINUTILS_VERSION.tar.xz" -o "binutils-$BINUTILS_VERSION.tar.xz"
  fi
  rm -rf "binutils-$BINUTILS_VERSION"
  tar -xf "binutils-$BINUTILS_VERSION.tar.xz"
  mkdir -p build-binutils
  cd build-binutils
  ../binutils-$BINUTILS_VERSION/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror
  make -j"$(nproc)"
  make install
  cd ..
  echo "Binutils installed."
else
  echo "Binutils already present."
fi

# 3) GCC (no libc; for kernel only)
if ! command -v "$TARGET-gcc" &>/dev/null; then
  echo "Building GCC $GCC_VERSION (this may take 30–60 minutes)..."
  if [[ ! -f "gcc-$GCC_VERSION.tar.xz" ]]; then
    curl -sL "$GNU_MIRROR/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.xz" -o "gcc-$GCC_VERSION.tar.xz"
  fi
  rm -rf "gcc-$GCC_VERSION"
  tar -xf "gcc-$GCC_VERSION.tar.xz"
  mkdir -p build-gcc
  cd build-gcc
  ../gcc-$GCC_VERSION/configure \
    --target=$TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ \
    --without-headers --with-newlib --disable-shared --disable-threads \
    --disable-libssp --disable-libgomp --disable-libmudflap
  make -j"$(nproc)" all-gcc all-target-libgcc
  make install-gcc install-target-libgcc
  cd ..
  echo "GCC installed."
else
  echo "GCC already present."
fi

echo ""
echo "Done. Add to your PATH:  export PATH=\"$PREFIX/bin:\$PATH\""
echo "Then run:  make check-toolchain  &&  make"
