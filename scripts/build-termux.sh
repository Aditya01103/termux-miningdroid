#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BACKEND="$ROOT/build/ccminer-verus"
PREFIX="${PREFIX:-/data/data/com.termux/files/usr}"
SSE2NEON="$BACKEND/sse2neon"

mkdir -p "$ROOT/build"

echo "[1/3] Checking Termux dependencies..."
echo "Install if needed:"
echo "pkg install git clang make autoconf automake libtool pkg-config openssl libcurl jansson"

if [ ! -d "$BACKEND/.git" ]; then
  echo "[2/3] Cloning VerusHash 2.2 CPU backend..."
  git clone --depth 1 --branch Verus2.2 \
    https://github.com/monkins1010/ccminer.git "$BACKEND"
else
  echo "[2/3] Updating VerusHash 2.2 CPU backend..."
  git -C "$BACKEND" fetch --depth 1 origin Verus2.2
  git -C "$BACKEND" reset --hard FETCH_HEAD
fi

echo "Backend commit:"
git -C "$BACKEND" rev-parse HEAD

echo "Preparing ARM64 SSE2NEON headers..."
if [ ! -d "$SSE2NEON/.git" ]; then
  git clone --depth 1 https://github.com/DLTcollab/sse2neon.git "$SSE2NEON"
else
  git -C "$SSE2NEON" pull --ff-only
fi

cd "$BACKEND"

echo "[2/3] Building backend..."

if [ -x ./build.sh ]; then
  ./build.sh
else
  [ -x ./autogen.sh ] && ./autogen.sh || true
  ./configure --with-curl="$PREFIX" --with-jansson="$PREFIX"
  make -j"$(nproc)"
fi

if [ ! -x ./ccminer ]; then
  echo "ERROR: backend build did not produce ./ccminer"
  exit 1
fi

cp ./ccminer "$ROOT/droider-backend"
chmod +x "$ROOT/droider-backend"

echo "[3/3] Building C launcher..."

clang -O2 -Wall -Wextra -std=c11 \
  "$ROOT/src/droider.c" \
  -o "$ROOT/droider"

chmod +x "$ROOT/droider"

echo
echo "Build complete:"
echo "  $ROOT/droider"
echo "  $ROOT/droider-backend"
echo
echo "Test:"
echo "  cd $ROOT"
echo "  ./droider -h"
echo
echo "Example:"
echo "  ./droider -a verushash -o stratum+tcp://POOL:PORT -u WALLET.worker -t 4 -p 50 -P x"
