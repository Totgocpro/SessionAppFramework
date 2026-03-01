#!/usr/bin/env bash
set -e

# 1. Setup Virtual Environment
if [ ! -d ".venv" ]; then
    echo ">>> Creating virtual environment..."
    python -m venv .venv
fi

source .venv/bin/activate

echo ">>> Installing python dependencies..."
pip install pybind11 --quiet

# 2. Build libsession-util first (if not done)
LIBSESSION_BUILD="libsession-util/Build"
if [ ! -f "$LIBSESSION_BUILD/src/libsession-util.a" ]; then
    echo ">>> Building libsession-util submodule..."
    cd libsession-util
    ./build.sh
    cd ..
fi

# 3. Build SessionAppFramework with Python bindings
BUILD_DIR="Build"
echo ">>> Building SessionAppFramework with Python bindings..."
mkdir -p "$BUILD_DIR"

# Determine path to libsession-util
LIBSESSION_DIR="$(pwd)/libsession-util"
OXENC_INC="${LIBSESSION_DIR}/external/oxen-libquic/external/oxen-encoding"
FMT_INC="${LIBSESSION_DIR}/external/oxen-logging/fmt/include"
SPDLOG_INC="${LIBSESSION_DIR}/external/oxen-logging/spdlog/include"
PROTO_INC="${LIBSESSION_DIR}/proto"

# Find pybind11 CMake directory
PYBIND11_CMAKE_DIR=$(python3 -c "import pybind11; print(pybind11.get_cmake_dir())")

# Collect static libs
LIBSESSION_LIBS=($(find "$LIBSESSION_DIR/Build" -name "*.a"))
LIBSESSION_LIBS_STR=$(IFS=';'; echo "${LIBSESSION_LIBS[*]}")

cmake -G Ninja -S . -B "$BUILD_DIR" \
      -D SAF_BUILD_PYTHON=ON \
      -D pybind11_DIR="$PYBIND11_CMAKE_DIR" \
      -D LIBSESSION_INCLUDE_DIRS="${LIBSESSION_DIR}/include;${OXENC_INC};${FMT_INC};${SPDLOG_INC};${PROTO_INC}" \
      -D LIBSESSION_LIBRARIES="${LIBSESSION_LIBS_STR}"

cmake --build "$BUILD_DIR" --parallel

# 4. Final step: link or copy the compiled module to current dir for easy import
SO_FILE=$(find "$BUILD_DIR" -name "session_saf*.so" | head -n 1)
if [ -z "$SO_FILE" ]; then
    echo ">>> Error: Could not find compiled session_saf module."
    exit 1
fi
cp "$SO_FILE" .

echo ">>> Build complete! session_saf module is ready."
echo ">>> Starting Python Group Bot..."
PYTHONPATH=. python examples/06_group_management_bot.py "$@"
