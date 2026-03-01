#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────
# build.sh  –  Auto-build everything for SessionAppFramework
# ─────────────────────────────────────────────────────────
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/Build"
LIBSESSION_DIR="${SCRIPT_DIR}/libsession-util"
LIBSESSION_BUILD="${LIBSESSION_DIR}/Build"

# ── Setup Colors ─────────────────────────────────────────
BLUE='\033[0;34m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

echo -e "${BLUE}>>> 1. Building libsession-util local dependencies...${NC}"

# 1. Compile libsession-util (locally if not done)
if [ ! -d "$LIBSESSION_BUILD" ] || [ ! -f "$LIBSESSION_BUILD/src/libsession-util.a" ]; then
    echo -e "${BLUE}   > Patching libsession-util/CMakeLists.txt...${NC}"
    # Remove/Comment out the line causing the error because libsession-util_src target doesn't exist
    sed -i 's/target_compile_options(libsession-util_src/ # target_compile_options(libsession-util_src/' "$LIBSESSION_DIR/CMakeLists.txt"

    echo -e "${BLUE}   > Configuring libsession-util...${NC}"
    mkdir -p "$LIBSESSION_BUILD"
    # Note: We build with Ninja for speed
    cmake -G Ninja -S "$LIBSESSION_DIR" -B "$LIBSESSION_BUILD" \
          -D STATIC_BUNDLE=ON \
          -D BUILD_STATIC_DEPS=ON \
          -D WITH_TESTS=OFF \
          -D CMAKE_CXX_FLAGS="-Wno-stringop-overflow"
    
    echo -e "${BLUE}   > Compiling libsession-util...${NC}"
    cmake --build "$LIBSESSION_BUILD" --parallel
else
    echo -e "${GREEN}   > libsession-util already built.${NC}"
fi

echo -e "${BLUE}>>> 2. Configuring SessionAppFramework...${NC}"

# Find internal headers required by libsession-util
OXENC_INC="${LIBSESSION_DIR}/external/oxen-libquic/external/oxen-encoding"
FMT_INC="${LIBSESSION_DIR}/external/oxen-logging/fmt/include"
SPDLOG_INC="${LIBSESSION_DIR}/external/oxen-logging/spdlog/include"
PROTO_INC="${LIBSESSION_DIR}/proto"

# Collect ALL static libraries from the build folder
LIBSESSION_LIBS=($(find "$LIBSESSION_BUILD" -name "*.a"))
LIBSESSION_LIBS_STR=$(IFS=';'; echo "${LIBSESSION_LIBS[*]}")

CMAKE_ARGS=(
    -G Ninja
    -S "${SCRIPT_DIR}"
    -B "${BUILD_DIR}"
    -D CMAKE_BUILD_TYPE="Release"
    -D SAF_BUILD_EXAMPLES=ON
    -D SAF_ENABLE_ONION=ON
    # Point directly to the local libsession-util build and its dependency headers
    -D LIBSESSION_INCLUDE_DIRS="${LIBSESSION_DIR}/include;${OXENC_INC};${FMT_INC};${SPDLOG_INC};${PROTO_INC}"
    -D LIBSESSION_LIBRARIES="${LIBSESSION_LIBS_STR}"
)

mkdir -p "$BUILD_DIR"
cmake "${CMAKE_ARGS[@]}"

echo -e "${BLUE}>>> 3. Compiling SessionAppFramework...${NC}"
cmake --build "${BUILD_DIR}" --parallel

echo -e "${GREEN}"
echo "✅  Build complete!"
echo "   Library:  ${BUILD_DIR}/libSessionAppFramework.a"
echo "   Examples: ${BUILD_DIR}/examples/"
echo -e "${NC}"
