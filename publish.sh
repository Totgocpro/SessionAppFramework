#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────
# publish.sh - Automate PyPI uploads
# ─────────────────────────────────────────────────────────
set -euo pipefail

# Terminal colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${BLUE}>>> Preparing session-saf upload...${NC}"

# 1. Cleanup
echo -e "${YELLOW}>>> 1. Cleaning up previous build artifacts...${NC}"
rm -rf dist/ build/ *.egg-info Build/ libsession-util/Build/

# 2. Virtual environment management (avoid externally-managed-environment error)
if [ -z "${VIRTUAL_ENV:-}" ]; then
    if [ ! -d ".venv-publish" ]; then
        echo -e "${YELLOW}>>> 2. Creating local virtual environment for publication tools...${NC}"
        python3 -m venv .venv-publish
    fi
    echo -e "${GREEN}>>> Using .venv-publish${NC}"
    export PATH="$(pwd)/.venv-publish/bin:$PATH"
fi

# 3. Tool verification
echo -e "${YELLOW}>>> 3. Updating build tools (build, twine)...${NC}"
pip install --upgrade build twine --quiet

# 4. Create source distribution (sdist)
echo -e "${YELLOW}>>> 4. Creating Source Distribution (sdist)...${NC}"
python3 -m build --sdist

# 5. Choose destination
echo -e "${BLUE}--------------------------------------------------${NC}"
echo -e "Where do you want to upload the package?"
echo -e "1) ${YELLOW}TestPyPI${NC} (For testing installation)"
echo -e "2) ${GREEN}PyPI Official${NC} (Production)"
echo -e "q) Quit"
read -p "Your choice [1/2/q]: " choice

case $choice in
    1) REPO_URL="--repository testpypi" ;;
    2) REPO_URL="" ;;
    q) exit 0 ;;
    *) echo -e "${RED}Invalid choice.${NC}"; exit 1 ;;
esac

# 6. Token Request
echo -e "${BLUE}--------------------------------------------------${NC}"
echo -e "Using default username: ${GREEN}__token__${NC}"
echo -e "Please paste your API Token (it will not be displayed):"
read -s -p "Token: " PYPI_TOKEN
echo ""

if [ -z "$PYPI_TOKEN" ]; then
    echo -e "${RED}Error: Token cannot be empty.${NC}"
    exit 1
fi

# 7. Upload
echo -e "${YELLOW}>>> 5. Uploading to PyPI...${NC}"
export TWINE_USERNAME="__token__"
export TWINE_PASSWORD="$PYPI_TOKEN"

if [ "$choice" == "1" ]; then
    python3 -m twine upload --repository testpypi dist/*
    echo -e "${GREEN}✅ Successfully uploaded to TestPyPI!${NC}"
    echo -e "To test installation: ${BLUE}pip install --index-url https://test.pypi.org/simple/ session-saf${NC}"
else
    python3 -m twine upload dist/*
    echo -e "${GREEN}✅ Successfully uploaded to Official PyPI!${NC}"
    echo -e "To install: ${BLUE}pip install session-saf${NC}"
fi
