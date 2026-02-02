#!/bin/bash
# ============================================================================
# Local Docker Testing Script for XOffsetDatastructure
# ============================================================================
# Purpose: Test locally before pushing to GitHub Actions
# Usage: ./scripts/local-docker-test.sh [--no-cache] [--interactive]

set -e

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Configuration
IMAGE_NAME="xoffset-clang-p2996:latest"
BUILD_DOCKER=0
NO_CACHE=""
INTERACTIVE=0

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --build)
            BUILD_DOCKER=1
            shift
            ;;
        --no-cache)
            NO_CACHE="--no-cache"
            shift
            ;;
        --interactive|-i)
            INTERACTIVE=1
            shift
            ;;
        --help|-h)
            echo ""
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --build            Build Docker image before testing"
            echo "  --no-cache         Build Docker without cache (slower)"
            echo "  --interactive, -i  Enter interactive shell instead of running tests"
            echo "  --help, -h         Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0                 # Run tests with existing image"
            echo "  $0 --build         # Build image then run tests"
            echo "  $0 -i              # Enter interactive shell for debugging"
            echo ""
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            exit 1
            ;;
    esac
done

echo ""
echo -e "${CYAN}======================================================================${NC}"
echo -e "${CYAN}  XOffsetDatastructure Local Docker Testing${NC}"
echo -e "${CYAN}======================================================================${NC}"
echo ""

# Check if Docker is running
if ! docker info > /dev/null 2>&1; then
    echo -e "${RED}Error: Docker is not running${NC}"
    echo "Please start Docker Desktop and try again"
    exit 1
fi

echo -e "${GREEN}✓ Docker is running${NC}"
echo ""

# Build Docker image if requested
if [ $BUILD_DOCKER -eq 1 ]; then
    echo -e "${BLUE}Building Docker image...${NC}"
    echo -e "${YELLOW}This may take 1-1.5 hours on first build${NC}"
    echo ""
    
    docker build $NO_CACHE -t $IMAGE_NAME .
    
    if [ $? -ne 0 ]; then
        echo ""
        echo -e "${RED}Docker build failed!${NC}"
        exit 1
    fi
    
    echo ""
    echo -e "${GREEN}✓ Docker image built successfully${NC}"
    echo ""
fi

# Check if image exists
if ! docker images | grep -q "xoffset-clang-p2996"; then
    echo -e "${YELLOW}Docker image not found. Building...${NC}"
    echo ""
    docker build -t $IMAGE_NAME .
    echo ""
fi

# Get absolute path to workspace
WORKSPACE_PATH="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo -e "${BLUE}Workspace: ${WORKSPACE_PATH}${NC}"
echo ""

# Interactive mode
if [ $INTERACTIVE -eq 1 ]; then
    echo -e "${CYAN}Entering interactive shell...${NC}"
    echo -e "${YELLOW}Tips:${NC}"
    echo "  - Run tests: ${CYAN}bash ./build.sh${NC}"
    echo "  - Debug build: ${CYAN}cd build && cmake .. && make -j4${NC}"
    echo "  - Exit shell: ${CYAN}exit${NC}"
    echo ""
    
    docker run --rm -it \
        -v "${WORKSPACE_PATH}:/workspace" \
        -w /workspace \
        $IMAGE_NAME \
        bash
    
    exit 0
fi

# Run tests
echo -e "${CYAN}======================================================================${NC}"
echo -e "${CYAN}  Running Tests in Docker Container${NC}"
echo -e "${CYAN}======================================================================${NC}"
echo ""

echo -e "${BLUE}Command:${NC}"
echo "  docker run --rm -v ${WORKSPACE_PATH}:/workspace -w /workspace $IMAGE_NAME bash ./build.sh"
echo ""

# Execute tests
START_TIME=$(date +%s)

docker run --rm \
    -v "${WORKSPACE_PATH}:/workspace" \
    -w /workspace \
    $IMAGE_NAME \
    bash ./build.sh

EXIT_CODE=$?
END_TIME=$(date +%s)
DURATION=$((END_TIME - START_TIME))

echo ""
echo -e "${CYAN}======================================================================${NC}"
echo -e "${CYAN}  Test Summary${NC}"
echo -e "${CYAN}======================================================================${NC}"
echo ""

MINUTES=$((DURATION / 60))
SECONDS=$((DURATION % 60))

echo -e "  Duration: ${CYAN}${MINUTES}m ${SECONDS}s${NC}"

if [ $EXIT_CODE -eq 0 ]; then
    echo -e "  Status: ${GREEN}✓ ALL TESTS PASSED${NC}"
    echo ""
    echo -e "${GREEN}Local tests completed successfully!${NC}"
    echo -e "${GREEN}You can now push to GitHub with confidence.${NC}"
else
    echo -e "  Status: ${RED}✗ TESTS FAILED${NC}"
    echo ""
    echo -e "${RED}Some tests failed. Please fix before pushing.${NC}"
    echo ""
    echo -e "${YELLOW}Debug tips:${NC}"
    echo "  1. Run with interactive mode: ${CYAN}$0 -i${NC}"
    echo "  2. Check specific test: ${CYAN}docker run ... bash -c 'cd build && ./bin/Release/test_basic_types'${NC}"
    echo "  3. Review build logs above for error details"
fi

echo ""
echo -e "${CYAN}======================================================================${NC}"
echo ""

exit $EXIT_CODE
