# ============================================================================
# Multi-stage Dockerfile for XOffsetDatastructure with Clang P2996
# ============================================================================

# ============================================================================
# Stage 1: Build Clang P2996 from source
# ============================================================================
FROM ubuntu:22.04 AS clang-builder

# Avoid interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    git \
    cmake \
    ninja-build \
    build-essential \
    python3 \
    python3-pip \
    lsb-release \
    wget \
    software-properties-common \
    gnupg \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Set up build directory
WORKDIR /tmp/clang-build

# Clone Clang P2996 repository
# Pin to specific commit for cache stability (update this hash periodically)
ARG CLANG_P2996_COMMIT=HEAD
RUN git clone -b p2996 --single-branch https://github.com/bloomberg/clang-p2996.git && \
    cd clang-p2996 && \
    if [ "$CLANG_P2996_COMMIT" != "HEAD" ]; then git checkout $CLANG_P2996_COMMIT; fi

# Configure and build Clang P2996
WORKDIR /tmp/clang-build/clang-p2996/build
RUN cmake -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_ENABLE_PROJECTS='clang' \
    -DLLVM_ENABLE_RUNTIMES='libcxx;libcxxabi;libunwind' \
    -DLLVM_TARGETS_TO_BUILD='X86' \
    -DCMAKE_INSTALL_PREFIX=/opt/clang-p2996 \
    -DLLVM_ENABLE_ASSERTIONS=OFF \
    -DLLVM_OPTIMIZED_TABLEGEN=ON \
    -DLLVM_BUILD_TESTS=OFF \
    -DLLVM_INCLUDE_TESTS=OFF \
    -DLIBCXX_ENABLE_EXPERIMENTAL_LIBRARY=ON \
    ../llvm

# Build with all available cores
RUN ninja -j$(nproc)

# Install to /opt/clang-p2996
RUN ninja install

# ============================================================================
# Stage 2: Runtime image with Clang P2996
# ============================================================================
FROM ubuntu:22.04 AS xoffset-dev

# Avoid interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install runtime dependencies including Boost
RUN apt-get update && apt-get install -y \
    git \
    cmake \
    ninja-build \
    build-essential \
    python3 \
    ca-certificates \
    libboost-all-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy Clang P2996 from builder stage
COPY --from=clang-builder /opt/clang-p2996 /opt/clang-p2996

# Set environment variables
ENV CC=/opt/clang-p2996/bin/clang
ENV CXX=/opt/clang-p2996/bin/clang++
ENV PATH=/opt/clang-p2996/bin:$PATH
ENV LD_LIBRARY_PATH=/opt/clang-p2996/lib:$LD_LIBRARY_PATH

# Verify installation
RUN clang++ --version && \
    echo "Clang P2996 installed successfully"

# Create working directory
WORKDIR /workspace

# Set default command
CMD ["/bin/bash"]

# ============================================================================
# Build instructions:
# 
# Build the image:
#   docker build -t xoffset-clang-p2996 .
#
# Run with local source mounted:
#   docker run -it -v $(pwd):/workspace xoffset-clang-p2996
#
# Build and test inside container:
#   ./build.sh
# ============================================================================
