# ============================================================================
# Makefile for XOffsetDatastructure2
# Simplifies building and testing
# ============================================================================

# Default target
.DEFAULT_GOAL := help

# Variables
BUILD_DIR := build
BUILD_TYPE := Release
NPROC := $(shell nproc 2>/dev/null || echo 4)

# Colors for output
COLOR_RESET := \033[0m
COLOR_GREEN := \033[0;32m
COLOR_YELLOW := \033[1;33m
COLOR_BLUE := \033[0;34m

# ============================================================================
# Help Target
# ============================================================================

.PHONY: help
help:
	@echo ""
	@echo "$(COLOR_BLUE)========================================$(COLOR_RESET)"
	@echo "$(COLOR_BLUE)  XOffsetDatastructure2 Build Helper$(COLOR_RESET)"
	@echo "$(COLOR_BLUE)========================================$(COLOR_RESET)"
	@echo ""
	@echo "$(COLOR_GREEN)Quick Start:$(COLOR_RESET)"
	@echo "  make all              - Configure, build, and test (basic)"
	@echo "  make all-reflection   - Configure, build, and test (with reflection)"
	@echo ""
	@echo "$(COLOR_GREEN)Configuration:$(COLOR_RESET)"
	@echo "  make config           - Configure project (C++20, no reflection)"
	@echo "  make config-reflection - Configure project (C++26, with reflection)"
	@echo "  make config-debug     - Configure for debug build"
	@echo ""
	@echo "$(COLOR_GREEN)Building:$(COLOR_RESET)"
	@echo "  make build            - Build all targets"
	@echo "  make build-fast       - Parallel build"
	@echo "  make build-tests      - Build only tests"
	@echo ""
	@echo "$(COLOR_GREEN)Testing:$(COLOR_RESET)"
	@echo "  make test             - Run all tests (CTest)"
	@echo "  make test-basic       - Run basic tests only"
	@echo "  make test-reflection  - Run reflection tests only"
	@echo "  make test-verbose     - Run tests with verbose output"
	@echo ""
	@echo "$(COLOR_GREEN)Reflection Tests:$(COLOR_RESET)"
	@echo "  make reflection-operators    - Run reflection operators test"
	@echo "  make reflection-iteration    - Run member iteration test"
	@echo "  make reflection-all          - Run all reflection tests"
	@echo ""
	@echo "$(COLOR_GREEN)Maintenance:$(COLOR_RESET)"
	@echo "  make clean            - Clean build directory"
	@echo "  make rebuild          - Clean and rebuild"
	@echo "  make fresh            - Fresh configuration and build"
	@echo ""
	@echo "$(COLOR_GREEN)Documentation:$(COLOR_RESET)"
	@echo "  make docs             - List all documentation files"
	@echo ""

# ============================================================================
# Quick Start Targets
# ============================================================================

.PHONY: all
all: config build test
	@echo "$(COLOR_GREEN)✓ All done!$(COLOR_RESET)"

.PHONY: all-reflection
all-reflection: config-reflection build test-reflection
	@echo "$(COLOR_GREEN)✓ All done (with reflection)!$(COLOR_RESET)"

# ============================================================================
# Configuration Targets
# ============================================================================

.PHONY: config
config:
	@echo "$(COLOR_YELLOW)Configuring project (C++20)...$(COLOR_RESET)"
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
	@echo "$(COLOR_GREEN)✓ Configuration complete$(COLOR_RESET)"

.PHONY: config-reflection
config-reflection:
	@echo "$(COLOR_YELLOW)Configuring project (C++26 with reflection)...$(COLOR_RESET)"
	cmake -B $(BUILD_DIR) \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DENABLE_REFLECTION_TESTS=ON
	@echo "$(COLOR_GREEN)✓ Configuration complete$(COLOR_RESET)"

.PHONY: config-debug
config-debug:
	@echo "$(COLOR_YELLOW)Configuring project (Debug mode)...$(COLOR_RESET)"
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug
	@echo "$(COLOR_GREEN)✓ Configuration complete$(COLOR_RESET)"

.PHONY: config-fresh
config-fresh:
	@echo "$(COLOR_YELLOW)Fresh configuration...$(COLOR_RESET)"
	cmake -B $(BUILD_DIR) --fresh
	@echo "$(COLOR_GREEN)✓ Configuration complete$(COLOR_RESET)"

# ============================================================================
# Build Targets
# ============================================================================

.PHONY: build
build:
	@echo "$(COLOR_YELLOW)Building project...$(COLOR_RESET)"
	cmake --build $(BUILD_DIR)
	@echo "$(COLOR_GREEN)✓ Build complete$(COLOR_RESET)"

.PHONY: build-fast
build-fast:
	@echo "$(COLOR_YELLOW)Building project (parallel, j=$(NPROC))...$(COLOR_RESET)"
	cmake --build $(BUILD_DIR) -j$(NPROC)
	@echo "$(COLOR_GREEN)✓ Build complete$(COLOR_RESET)"

.PHONY: build-tests
build-tests:
	@echo "$(COLOR_YELLOW)Building tests only...$(COLOR_RESET)"
	cmake --build $(BUILD_DIR) --target test_basic_types
	cmake --build $(BUILD_DIR) --target test_vector
	cmake --build $(BUILD_DIR) --target test_map_set
	cmake --build $(BUILD_DIR) --target test_nested
	cmake --build $(BUILD_DIR) --target test_compaction
	cmake --build $(BUILD_DIR) --target test_modify
	@echo "$(COLOR_GREEN)✓ Tests built$(COLOR_RESET)"

.PHONY: build-reflection-tests
build-reflection-tests:
	@echo "$(COLOR_YELLOW)Building reflection tests only...$(COLOR_RESET)"
	cmake --build $(BUILD_DIR) --target test_reflection_operators
	cmake --build $(BUILD_DIR) --target test_member_iteration
	cmake --build $(BUILD_DIR) --target test_reflection_type_signature
	cmake --build $(BUILD_DIR) --target test_splice_operations
	cmake --build $(BUILD_DIR) --target test_type_introspection
	cmake --build $(BUILD_DIR) --target test_reflection_compaction
	cmake --build $(BUILD_DIR) --target test_reflection_serialization
	cmake --build $(BUILD_DIR) --target test_reflection_comparison
	@echo "$(COLOR_GREEN)✓ Reflection tests built$(COLOR_RESET)"

# ============================================================================
# Test Targets
# ============================================================================

.PHONY: test
test:
	@echo "$(COLOR_YELLOW)Running all tests...$(COLOR_RESET)"
	cd $(BUILD_DIR) && ctest
	@echo "$(COLOR_GREEN)✓ Tests complete$(COLOR_RESET)"

.PHONY: test-verbose
test-verbose:
	@echo "$(COLOR_YELLOW)Running all tests (verbose)...$(COLOR_RESET)"
	cd $(BUILD_DIR) && ctest --verbose
	@echo "$(COLOR_GREEN)✓ Tests complete$(COLOR_RESET)"

.PHONY: test-basic
test-basic:
	@echo "$(COLOR_YELLOW)Running basic tests...$(COLOR_RESET)"
	cd $(BUILD_DIR) && ctest -R "BasicTypes|VectorOps|MapSetOps|NestedStructures|MemoryCompaction|DataModification"
	@echo "$(COLOR_GREEN)✓ Basic tests complete$(COLOR_RESET)"

.PHONY: test-reflection
test-reflection:
	@echo "$(COLOR_YELLOW)Running reflection tests...$(COLOR_RESET)"
	cd $(BUILD_DIR) && ctest -R "test_reflection"
	@echo "$(COLOR_GREEN)✓ Reflection tests complete$(COLOR_RESET)"

# ============================================================================
# Individual Reflection Test Targets
# ============================================================================

.PHONY: reflection-operators
reflection-operators:
	@echo "$(COLOR_YELLOW)Running reflection operators test...$(COLOR_RESET)"
	./$(BUILD_DIR)/bin/$(BUILD_TYPE)/test_reflection_operators

.PHONY: reflection-iteration
reflection-iteration:
	@echo "$(COLOR_YELLOW)Running member iteration test...$(COLOR_RESET)"
	./$(BUILD_DIR)/bin/$(BUILD_TYPE)/test_member_iteration

.PHONY: reflection-type-signature
reflection-type-signature:
	@echo "$(COLOR_YELLOW)Running type signature test...$(COLOR_RESET)"
	./$(BUILD_DIR)/bin/$(BUILD_TYPE)/test_reflection_type_signature

.PHONY: reflection-splice
reflection-splice:
	@echo "$(COLOR_YELLOW)Running splice operations test...$(COLOR_RESET)"
	./$(BUILD_DIR)/bin/$(BUILD_TYPE)/test_splice_operations

.PHONY: reflection-introspection
reflection-introspection:
	@echo "$(COLOR_YELLOW)Running type introspection test...$(COLOR_RESET)"
	./$(BUILD_DIR)/bin/$(BUILD_TYPE)/test_type_introspection

.PHONY: reflection-compaction
reflection-compaction:
	@echo "$(COLOR_YELLOW)Running reflection compaction test...$(COLOR_RESET)"
	./$(BUILD_DIR)/bin/$(BUILD_TYPE)/test_reflection_compaction

.PHONY: reflection-serialization
reflection-serialization:
	@echo "$(COLOR_YELLOW)Running reflection serialization test...$(COLOR_RESET)"
	./$(BUILD_DIR)/bin/$(BUILD_TYPE)/test_reflection_serialization

.PHONY: reflection-comparison
reflection-comparison:
	@echo "$(COLOR_YELLOW)Running reflection comparison test...$(COLOR_RESET)"
	./$(BUILD_DIR)/bin/$(BUILD_TYPE)/test_reflection_comparison

.PHONY: reflection-all
reflection-all: reflection-operators reflection-iteration reflection-type-signature reflection-splice reflection-introspection reflection-compaction reflection-serialization reflection-comparison
	@echo "$(COLOR_GREEN)✓ All reflection tests run$(COLOR_RESET)"

# ============================================================================
# Maintenance Targets
# ============================================================================

.PHONY: clean
clean:
	@echo "$(COLOR_YELLOW)Cleaning build directory...$(COLOR_RESET)"
	rm -rf $(BUILD_DIR)
	@echo "$(COLOR_GREEN)✓ Clean complete$(COLOR_RESET)"

.PHONY: rebuild
rebuild: clean config build
	@echo "$(COLOR_GREEN)✓ Rebuild complete$(COLOR_RESET)"

.PHONY: fresh
fresh: clean config-fresh build
	@echo "$(COLOR_GREEN)✓ Fresh build complete$(COLOR_RESET)"

# ============================================================================
# Documentation Targets
# ============================================================================

.PHONY: docs
docs:
	@echo ""
	@echo "$(COLOR_BLUE)Available Documentation:$(COLOR_RESET)"
	@echo "  BUILD_AND_RUN_GUIDE.md          - Detailed build guide"
	@echo "  QUICK_BUILD_REFERENCE.md        - Quick command reference"
	@echo "  REFLECTION_QUICKSTART.md        - Reflection quick start"
	@echo "  REFLECTION_TESTS_SUMMARY.md     - Test summary"
	@echo "  REFLECTION_TEST_RECOMMENDATIONS.md - Full recommendations"
	@echo "  REFLECTION_COMPLETE_REPORT.md   - Completion report"
	@echo "  tests/README.md                 - Tests documentation"
	@echo ""

# ============================================================================
# Utility Targets
# ============================================================================

.PHONY: list-tests
list-tests:
	@echo "$(COLOR_YELLOW)Listing all tests...$(COLOR_RESET)"
	cd $(BUILD_DIR) && ctest --show-only

.PHONY: install-deps
install-deps:
	@echo "$(COLOR_YELLOW)Note: Boost is included in external/boost/$(COLOR_RESET)"
	@echo "No additional dependencies required"

.PHONY: info
info:
	@echo ""
	@echo "$(COLOR_BLUE)Build Information:$(COLOR_RESET)"
	@echo "  Build Directory: $(BUILD_DIR)"
	@echo "  Build Type: $(BUILD_TYPE)"
	@echo "  Parallel Jobs: $(NPROC)"
	@echo ""
	@if [ -d "$(BUILD_DIR)" ]; then \
		echo "$(COLOR_GREEN)Build directory exists$(COLOR_RESET)"; \
		echo "Executables:"; \
		find $(BUILD_DIR)/bin -type f -executable 2>/dev/null | sed 's/^/  /' || echo "  (none found)"; \
	else \
		echo "$(COLOR_YELLOW)Build directory does not exist$(COLOR_RESET)"; \
		echo "Run 'make config' first"; \
	fi
	@echo ""
