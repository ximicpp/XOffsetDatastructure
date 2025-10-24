#!/bin/bash
# ============================================================================
# Upgrade CMake to Latest Version (using pre-built binary)
# ============================================================================

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}=========================================${NC}"
echo -e "${BLUE}  Upgrade CMake to Latest Version${NC}"
echo -e "${BLUE}=========================================${NC}"
echo

# Check current version
CURRENT_VERSION=$(cmake --version 2>/dev/null | head -1 | awk '{print $3}')
echo "Current CMake version: $CURRENT_VERSION"
echo "Required CMake version: 3.20.0+"
echo

# Remove old version
echo -e "${YELLOW}[1/4] Removing old CMake...${NC}"
sudo apt remove --purge -y cmake
sudo apt autoremove -y
echo

# Install dependencies
echo -e "${YELLOW}[2/4] Installing dependencies...${NC}"
sudo apt update
sudo apt install -y wget
echo

# Download pre-built CMake binary (much faster!)
echo -e "${YELLOW}[3/4] Downloading pre-built CMake 3.27.7...${NC}"
CMAKE_VERSION="3.27.7"
cd /tmp

# Download pre-built version
wget https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz

echo "Extracting..."
tar -xzf cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz
echo

# Install
echo -e "${YELLOW}[4/4] Installing CMake...${NC}"

# Remove conflicting man directory if it exists
if [ -e /usr/local/man ] && [ ! -d /usr/local/man ]; then
    sudo rm -f /usr/local/man
fi

# Install CMake files
cd cmake-${CMAKE_VERSION}-linux-x86_64
sudo cp -r bin/* /usr/local/bin/ 2>/dev/null || true
sudo cp -r share/* /usr/local/share/ 2>/dev/null || true
sudo cp -r doc/* /usr/local/doc/ 2>/dev/null || true
if [ -d man ]; then
    sudo mkdir -p /usr/local/man
    sudo cp -r man/* /usr/local/man/ 2>/dev/null || true
fi
cd ..

# Update symlink
sudo ln -sf /usr/local/bin/cmake /usr/bin/cmake

# Cleanup
cd /tmp
rm -rf cmake-${CMAKE_VERSION}-linux-x86_64 cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz

echo
echo -e "${GREEN}=========================================${NC}"
echo -e "${GREEN}  CMake Upgrade Complete!${NC}"
echo -e "${GREEN}=========================================${NC}"
echo

# Verify
NEW_VERSION=$(cmake --version | head -1 | awk '{print $3}')
echo "New CMake version: $NEW_VERSION"
echo

if [ "$(printf '%s\n' "3.20.0" "$NEW_VERSION" | sort -V | head -n1)" = "3.20.0" ]; then
    echo -e "${GREEN}[OK]CMake version is sufficient for Clang P2996${NC}"
else
    echo -e "${RED}[WARN] CMake version might still be insufficient${NC}"
fi

echo
echo "You can now run:"
echo "  wsl_build_clang_p2996.bat"