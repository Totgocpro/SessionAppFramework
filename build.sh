#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────
# build.sh  –  Auto-build everything for SessionAppFramework
# ─────────────────────────────────────────────────────────
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
    # Use Windows-style paths for CMake on MSYS2
    SCRIPT_DIR="$(pwd -W)"
fi
BUILD_DIR="${SCRIPT_DIR}/Build"
LIBSESSION_DIR="${SCRIPT_DIR}/libsession-util"
LIBSESSION_BUILD="${LIBSESSION_DIR}/Build"

# Determine build generator
GENERATOR="Unix Makefiles"
if command -v ninja >/dev/null 2>&1; then
    GENERATOR="Ninja"
fi

echo ">>> Using generator: ${GENERATOR}"

# Detect available python
PYTHON="python3"
if ! command -v python3 >/dev/null 2>&1; then
    PYTHON="python"
fi

# 1. Compile libsession-util
echo ">>> 1. Building libsession-util local dependencies..."

BUILD_STATIC_DEPS="ON"
STATIC_BUNDLE="ON"
ENABLE_ONIONREQ="${SAF_ENABLE_ONION:-ON}"

if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]] || [[ "${SAF_USE_SYSTEM_DEPS:-}" == "ON" ]]; then
    # On Windows/MSYS2, building static deps from source via libsession-util is broken.
    # On Linux CI, we prefer system deps to speed up the build.
    BUILD_STATIC_DEPS="OFF"
    STATIC_BUNDLE="OFF"

    # Force regeneration of proto files to avoid version mismatch with system protobuf
    echo "   > Regenerating proto files using system protoc..."
    (cd libsession-util/proto && protoc --cpp_out=. SessionProtos.proto WebSocketResources.proto)
fi

if [ ! -f "libsession-util/CMakeLists.txt" ]; then
    echo "❌ Error: libsession-util/CMakeLists.txt not found!"
    ls -F
    exit 1
fi

# Always try to patch to be safe
$PYTHON -c "import sys; content = open('libsession-util/CMakeLists.txt').read(); open('libsession-util/CMakeLists.txt', 'w').write(content.replace('target_compile_options(libsession-util_src', '# target_compile_options(libsession-util_src'))"

if [ ! -d "$LIBSESSION_BUILD" ] || [ ! -f "$LIBSESSION_BUILD/src/libsession-util.a" ] || [ ! -f "$LIBSESSION_BUILD/src/libsession-crypto.a" ]; then
    echo "   > Configuring libsession-util..."
    mkdir -p "$LIBSESSION_BUILD"
    cmake -G "${GENERATOR}" -S "$LIBSESSION_DIR" -B "$LIBSESSION_BUILD" \
          -D STATIC_BUNDLE="${STATIC_BUNDLE}" \
          -D BUILD_STATIC_DEPS="${BUILD_STATIC_DEPS}" \
          -D ENABLE_ONIONREQ="${ENABLE_ONIONREQ}" \
          -D WITH_TESTS=OFF \
          -D CMAKE_CXX_FLAGS="-Wno-stringop-overflow"
    
    echo "   > Compiling libsession-util..."
    cmake --build "$LIBSESSION_BUILD" --parallel
else
    echo "   > libsession-util already built."
fi

if [[ "${SAF_SKIP_MAIN_BUILD:-}" == "ON" ]]; then
    echo ">>> Skipping SessionAppFramework main build as requested (SAF_SKIP_MAIN_BUILD=ON)"
    exit 0
fi

echo ">>> 2. Configuring SessionAppFramework..."

# Find internal headers required by libsession-util
OXENC_INC="${LIBSESSION_DIR}/external/oxen-libquic/external/oxen-encoding"
FMT_INC="${LIBSESSION_DIR}/external/oxen-logging/fmt/include"
SPDLOG_INC="${LIBSESSION_DIR}/external/oxen-logging/spdlog/include"
PROTO_INC="${LIBSESSION_DIR}/proto"

# Collect ALL static libraries from the build folder
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
    # Windows/MinGW: look for .a and .lib and convert to Windows paths
    LIBSESSION_LIBS=($(find "$LIBSESSION_BUILD" -name "*.a" -o -name "*.lib" | xargs cygpath -m))
else
    # Linux/Mac
    LIBSESSION_LIBS=($(find "$LIBSESSION_BUILD" -name "*.a"))
fi
LIBSESSION_LIBS_STR=$(IFS=';'; echo "${LIBSESSION_LIBS[*]}")

mkdir -p "$BUILD_DIR"
cmake -G "${GENERATOR}" \
    -S "${SCRIPT_DIR}" \
    -B "${BUILD_DIR}" \
    -D CMAKE_BUILD_TYPE="Release" \
    -D SAF_BUILD_EXAMPLES=ON \
    -D SAF_ENABLE_ONION="${ENABLE_ONIONREQ}" \
    -D LIBSESSION_ROOT="${LIBSESSION_DIR}" \
    -D LIBSESSION_INCLUDE_DIRS="${LIBSESSION_DIR}/include;${OXENC_INC};${FMT_INC};${SPDLOG_INC};${PROTO_INC}" \
    -D LIBSESSION_LIBRARIES="${LIBSESSION_LIBS_STR}"

echo ">>> 3. Compiling SessionAppFramework..."
cmake --build "${BUILD_DIR}" --parallel

echo "✅  Build complete!"
