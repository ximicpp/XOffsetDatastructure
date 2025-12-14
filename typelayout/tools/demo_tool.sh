#!/bin/bash

# demo_tool.sh - Build and demo typelayout_tool
# For Linux/WSL with Clang P2996 (Bloomberg fork)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INCLUDE_DIR="${SCRIPT_DIR}/../include"
OUTPUT_DIR="${SCRIPT_DIR}/output"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Check for Clang P2996
CLANG_P2996="$HOME/clang-p2996-install/bin/clang++"
if [ ! -f "$CLANG_P2996" ]; then
    echo -e "${RED}Error: Clang P2996 not found at $CLANG_P2996${NC}"
    exit 1
fi

echo -e "${CYAN}=== TypeLayout Tool Demo ===${NC}\n"

# Build
echo -e "${BLUE}[1/4] Building typelayout-tool...${NC}"
cd "$SCRIPT_DIR"
$CLANG_P2996 -std=c++26 -freflection -O2 \
    -I"$INCLUDE_DIR" \
    typelayout_tool.cpp -o typelayout-tool \
    -stdlib=libc++ \
    -L$HOME/clang-p2996-install/lib \
    -Wl,-rpath,$HOME/clang-p2996-install/lib

echo -e "${GREEN}[OK] Build completed${NC}\n"

# Generate signatures
mkdir -p "$OUTPUT_DIR"

echo -e "${BLUE}[2/4] Generating signatures...${NC}"
./typelayout-tool generate -o "$OUTPUT_DIR/platform_a.sig"
echo -e "Output: ${YELLOW}$OUTPUT_DIR/platform_a.sig${NC}"
cat "$OUTPUT_DIR/platform_a.sig"
echo ""

# Create mock signature for comparison demo
echo -e "${BLUE}[3/4] Creating mock platform_b signature (with mismatch)...${NC}"
cat > "$OUTPUT_DIR/platform_b.sig" << 'EOF'
shared::Point 8:4{i32@0,i32@4}
shared::Header 24:8{u32@0,u32@4,u64@8,u32@16,u32@20}
shared::Message 88:8{24:8{u32@0,u32@4,u64@8,u32@16,u32@20}@0,i32@24,c[64]@28}
shared::Config 48:8{u32@0,f64@4,i32@12,c[32]@16}
EOF
echo -e "Output: ${YELLOW}$OUTPUT_DIR/platform_b.sig${NC}"
cat "$OUTPUT_DIR/platform_b.sig"
echo ""

# Compare
echo -e "${BLUE}[4/4] Comparing signatures...${NC}"
echo -e "${CYAN}$ ./typelayout-tool compare platform_a.sig platform_b.sig${NC}"
./typelayout-tool compare "$OUTPUT_DIR/platform_a.sig" "$OUTPUT_DIR/platform_b.sig" || true

echo -e "\n${GREEN}[OK] Demo completed${NC}"
echo -e "Files in $OUTPUT_DIR:"
ls -la "$OUTPUT_DIR"
