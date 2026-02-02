#!/bin/bash
# ============================================================================
# Local Docker Build with China Mirror
# ============================================================================

set -e

GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

echo ""
echo -e "${CYAN}======================================================================${NC}"
echo -e "${CYAN}  Building Docker Image (China Mirror)${NC}"
echo -e "${CYAN}======================================================================${NC}"
echo ""

# Create temporary Dockerfile with mirror
echo -e "${BLUE}Creating Dockerfile with China mirror...${NC}"

cat > Dockerfile.cn << 'EOF'
# Use Aliyun mirror
FROM registry.cn-hangzhou.aliyuncs.com/acs/ubuntu:22.04 AS builder

# Set mirror for apt
RUN sed -i 's/archive.ubuntu.com/mirrors.aliyun.com/g' /etc/apt/sources.list && \
    sed -i 's/security.ubuntu.com/mirrors.aliyun.com/g' /etc/apt/sources.list

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    python3 \
    ninja-build \
    libboost-all-dev \
    && rm -rf /var/lib/apt/lists/*

# Build Clang P2996
WORKDIR /tmp
RUN git clone --depth 1 https://github.com/bloomberg/clang-p2996.git llvm-project || \
    git clone --depth 1 https://gitee.com/mirrors/clang-p2996.git llvm-project

RUN cmake -S llvm-project/llvm -B build \
    -DLLVM_ENABLE_PROJECTS="clang" \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_ENABLE_RTTI=ON \
    -DLLVM_TARGETS_TO_BUILD="X86" \
    -G Ninja

RUN cmake --build build -j$(nproc)
RUN cmake --install build --prefix /usr/local

# Runtime image
FROM registry.cn-hangzhou.aliyuncs.com/acs/ubuntu:22.04 AS xoffset-dev

RUN sed -i 's/archive.ubuntu.com/mirrors.aliyun.com/g' /etc/apt/sources.list && \
    sed -i 's/security.ubuntu.com/mirrors.aliyun.com/g' /etc/apt/sources.list

RUN apt-get update && apt-get install -y \
    libboost-all-dev \
    cmake \
    build-essential \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /usr/local /usr/local

ENV PATH="/usr/local/bin:${PATH}"
ENV LD_LIBRARY_PATH="/usr/local/lib:${LD_LIBRARY_PATH}"

WORKDIR /workspace
EOF

echo -e "${GREEN}✓ Dockerfile.cn created${NC}"
echo ""

# Build
echo -e "${BLUE}Starting Docker build (this will take 1-1.5 hours)...${NC}"
echo ""

docker build -f Dockerfile.cn -t xoffset-clang-p2996:latest .

echo ""
echo -e "${GREEN}======================================================================${NC}"
echo -e "${GREEN}  Docker Build Complete!${NC}"
echo -e "${GREEN}======================================================================${NC}"
echo ""

# Clean up
rm -f Dockerfile.cn

echo -e "${BLUE}Test the image:${NC}"
echo "  docker run --rm xoffset-clang-p2996:latest clang++ --version"
echo ""
