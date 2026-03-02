#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────
# test_wheels_local.sh - Run cibuildwheel locally for testing
# ─────────────────────────────────────────────────────────
set -euo pipefail

echo ">>> Detecting platform..."
OS_NAME=$(uname -s)
PLATFORM=""

case "${OS_NAME}" in
    Linux*)     PLATFORM="linux";;
    Darwin*)    PLATFORM="macos";;
    CYGWIN*|MINGW*|MSYS*) PLATFORM="windows";;
    *)          PLATFORM="unknown";;
esac

echo ">>> Platform: ${PLATFORM}"

# Clean up leftover binaries that might confuse auditwheel
echo ">>> Cleaning up previous build artifacts..."
rm -f *.so *.dylib *.dll

# Handle externally managed environments by using a dedicated venv for the tool
CIBW_BIN="cibuildwheel"
if ! command -v cibuildwheel &> /dev/null; then
    if [ ! -d ".venv-cibw" ]; then
        echo ">>> cibuildwheel not found. Creating a local venv for it..."
        python3 -m venv .venv-cibw
        .venv-cibw/bin/pip install cibuildwheel
    fi
    CIBW_BIN="$(pwd)/.venv-cibw/bin/cibuildwheel"
else
    CIBW_BIN=$(command -v cibuildwheel)
fi

if [[ "${PLATFORM}" == "linux" ]]; then
    # Check for docker/podman
    CONTAINER_ENGINE=""
    USE_SUDO=""
    
    if command -v docker &> /dev/null; then
        CONTAINER_ENGINE="docker"
        # Check for permission denied
        if ! docker version &>/dev/null; then
            echo ">>> Docker permission denied. I'll use sudo for the command."
            USE_SUDO="sudo"
        fi
    elif command -v podman &> /dev/null; then
        CONTAINER_ENGINE="podman"
    fi

    if [ -z "${CONTAINER_ENGINE}" ]; then
        echo "❌ Error: Docker or Podman is required for Linux wheel builds."
        echo "   Please install docker ('sudo pacman -S docker') or podman ('sudo pacman -S podman')."
        exit 1
    fi

    echo ">>> Running cibuildwheel for Linux using ${CONTAINER_ENGINE}..."
    if [ -n "${USE_SUDO}" ]; then
        # Use -E to preserve environment variables
        sudo -E "${CIBW_BIN}" --platform linux
    else
        "${CIBW_BIN}" --platform linux
    fi
elif [[ "${PLATFORM}" == "macos" ]]; then
    echo ">>> Running cibuildwheel for macOS..."
    "${CIBW_BIN}" --platform macos
elif [[ "${PLATFORM}" == "windows" ]]; then
    echo ">>> Running cibuildwheel for Windows..."
    "${CIBW_BIN}" --platform windows
else
    echo "❌ Error: Unsupported platform ${OS_NAME}"
    exit 1
fi
