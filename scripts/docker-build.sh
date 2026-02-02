#!/bin/bash
# ============================================================================
# Docker Build Script for XOffsetDatastructure
# ============================================================================

set -e

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  XOffsetDatastructure Docker Build${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Default image name
IMAGE_NAME="xoffset-clang-p2996:latest"

# Parse arguments
BUILD_ARGS=""
while [[ $# -gt 0 ]]; do
    case $1 in
        --commit)
            BUILD_ARGS="$BUILD_ARGS --build-arg CLANG_P2996_COMMIT=$2"
            shift 2
            ;;
        --no-cache)
            BUILD_ARGS="$BUILD_ARGS --no-cache"
            shift
            ;;
        *)
            echo -e "${YELLOW}Unknown option: $1${NC}"
            shift
            ;;
    esac
done

echo -e "${GREEN}Building Docker image: $IMAGE_NAME${NC}"
echo ""

# Build image
docker build $BUILD_ARGS -t $IMAGE_NAME .

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  Build Complete!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo "To run the container:"
echo "  docker run -it -v \$(pwd):/workspace $IMAGE_NAME"
echo ""
echo "Or use docker-compose:"
echo "  docker-compose run --rm xoffset-dev"
echo ""
