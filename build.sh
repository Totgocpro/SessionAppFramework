#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────
# build.sh  –  Auto-build everything for SessionAppFramework
# ─────────────────────────────────────────────────────────
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
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

if [ ! -f "libsession-util/CMakeLists.txt" ]; then
    echo "❌ Error: libsession-util/CMakeLists.txt not found!"
    echo "Current directory content:"
    ls -F
    exit 1
fi

# Always try to patch to be safe - using relative path for Windows compatibility
$PYTHON -c "import sys; content = open('libsession-util/CMakeLists.txt').read(); open('libsession-util/CMakeLists.txt', 'w').write(content.replace('target_compile_options(libsession-util_src', '# target_compile_options(libsession-util_src'))"

if [ ! -d "$LIBSESSION_BUILD" ] || [ ! -f "$LIBSESSION_BUILD/src/libsession-util.a" ]; then
    echo "   > Configuring libsession-util..."
    mkdir -p "$LIBSESSION_BUILD"
    cmake -G "${GENERATOR}" -S "$LIBSESSION_DIR" -B "$LIBSESSION_BUILD" \
          -D STATIC_BUNDLE=ON \
          -D BUILD_STATIC_DEPS=ON \
          -D WITH_TESTS=OFF \
          -D CMAKE_CXX_FLAGS="-Wno-stringop-overflow"
    
    echo "   > Compiling libsession-util..."
    cmake --build "$LIBSESSION_BUILD" --parallel
else
    echo "   > libsession-util already built."
fi

echo ">>> 2. Configuring SessionAppFramework..."

# Find internal headers required by libsession-util
OXENC_INC="${LIBSESSION_DIR}/external/oxen-libquic/external/oxen-encoding"
FMT_INC="${LIBSESSION_DIR}/external/oxen-logging/fmt/include"
SPDLOG_INC="${LIBSESSION_DIR}/external/oxen-logging/spdlog/include"
PROTO_INC="${LIBSESSION_DIR}/proto"

# Collect ALL static libraries from the build folder
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
    # Windows/MinGW: look for .a and .lib
    LIBSESSION_LIBS=($(find "$LIBSESSION_BUILD" -name "*.a" -o -name "*.lib"))
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
    -D SAF_ENABLE_ONION=ON \
    -D LIBSESSION_ROOT="${LIBSESSION_DIR}" \
    -D LIBSESSION_INCLUDE_DIRS="${LIBSESSION_DIR}/include;${OXENC_INC};${FMT_INC};${SPDLOG_INC};${PROTO_INC}" \
    -D LIBSESSION_LIBRARIES="${LIBSESSION_LIBS_STR}"

echo ">>> 3. Compiling SessionAppFramework..."
cmake --build "${BUILD_DIR}" --parallel

echo "✅  Build complete!"
