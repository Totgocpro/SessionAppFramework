#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────
# test_wheels_local.sh - GitHub Actions Simulator
# ─────────────────────────────────────────────────────────
set -euo pipefail

echo ">>> 🔍 Detecting platform..."
OS_NAME=$(uname -s)
PLATFORM=""

case "${OS_NAME}" in
    Linux*)     PLATFORM="linux";;
    Darwin*)    PLATFORM="macos";;
    CYGWIN*|MINGW*|MSYS*) PLATFORM="windows";;
    *)          PLATFORM="unknown";;
esac

echo ">>> 💻 Platform: ${PLATFORM}"

# 1. Clean environment (Crucial for GitHub Simulation)
echo ">>> 🧹 Cleaning up previous build artifacts (LTO & auditwheel safety)..."
rm -f *.so *.dylib *.dll
rm -rf Build/ libsession-util/Build/ wheelhouse/

# 2. Setup Virtual Environment for cibuildwheel
CIBW_BIN="cibuildwheel"
if ! command -v cibuildwheel &> /dev/null; then
    if [ ! -d ".venv-cibw" ]; then
        echo ">>> 📦 cibuildwheel not found. Creating a local venv for it..."
        python3 -m venv .venv-cibw
        .venv-cibw/bin/pip install cibuildwheel
    fi
    CIBW_BIN="$(pwd)/.venv-cibw/bin/cibuildwheel"
else
    CIBW_BIN=$(command -v cibuildwheel)
fi

# 3. GitHub Environment Simulation
# These variables match exactly what we set in wheels.yml
export CIBW_BUILD_VERBOSITY=1
export SAF_USE_SYSTEM_DEPS=ON
export SAF_SKIP_MAIN_BUILD=ON
export SAF_ENABLE_ONION=OFF

echo ">>> 🚀 Starting GitHub Simulation for ${PLATFORM}..."

if [[ "${PLATFORM}" == "linux" ]]; then
    CONTAINER_ENGINE=""
    USE_SUDO=""
    if command -v docker &> /dev/null; then
        CONTAINER_ENGINE="docker"
        if ! docker version &>/dev/null; then
            echo ">>> 🛡️ Docker permission denied. Using sudo."
            USE_SUDO="sudo -E"
        fi
    elif command -v podman &> /dev/null; then
        CONTAINER_ENGINE="podman"
    fi

    if [ -z "${CONTAINER_ENGINE}" ]; then
        echo "❌ Error: Docker or Podman is required for Linux."
        exit 1
    fi
    ${USE_SUDO} "${CIBW_BIN}" --platform linux

elif [[ "${PLATFORM}" == "macos" ]]; then
    # On Mac, we just run it. pyproject.toml handles brew install.
    "${CIBW_BIN}" --platform macos

elif [[ "${PLATFORM}" == "windows" ]]; then
    echo ">>> 🚀 Starting GitHub Simulation for ${PLATFORM}..."
    # On Windows, we just run it. pyproject.toml handles bash -c.
    "${CIBW_BIN}" --platform windows

else
    echo "❌ Error: Unsupported platform ${OS_NAME}"
    exit 1
fi

echo "✅ Simulation finished! Check the 'wheelhouse/' directory."
