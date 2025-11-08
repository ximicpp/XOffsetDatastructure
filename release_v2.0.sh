#!/bin/bash

# ==============================================================================
# XOffsetDatastructure v2.0 Release Script
# ==============================================================================
# This script creates two independent release branches without commit history:
#   - v2.0-practical: Production-ready with Boost.PFR
#   - v2.0-cpp26: Experimental with C++26 reflection
# ==============================================================================

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
PRACTICAL_VERSION="v2.0-practical"
CPP26_VERSION="v2.0-cpp26"
PRACTICAL_BRANCH="release/v2.0-practical"
CPP26_BRANCH="release/v2.0-cpp26"

# ==============================================================================
# Utility Functions
# ==============================================================================

print_header() {
    echo -e "\n${BLUE}═══════════════════════════════════════════════════════════════${NC}"
    echo -e "${BLUE}  $1${NC}"
    echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}\n"
}

print_step() {
    echo -e "${GREEN}▶${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}⚠${NC} $1"
}

print_error() {
    echo -e "${RED}✗${NC} $1"
}

print_success() {
    echo -e "${GREEN}✓${NC} $1"
}

check_clean_state() {
    if [[ -n $(git status --porcelain) ]]; then
        print_error "Working directory is not clean. Please commit or stash changes first."
        exit 1
    fi
}

branch_exists() {
    git rev-parse --verify "$1" >/dev/null 2>&1
}

tag_exists() {
    git rev-parse --verify "refs/tags/$1" >/dev/null 2>&1
}

# ==============================================================================
# Pre-flight Checks
# ==============================================================================

preflight_check() {
    print_header "Pre-flight Checks"
    
    print_step "Checking Git repository..."
    if ! git rev-parse --git-dir > /dev/null 2>&1; then
        print_error "Not a Git repository"
        exit 1
    fi
    print_success "Git repository OK"
    
    print_step "Checking source branches..."
    if ! branch_exists "next_practical"; then
        print_error "Branch 'next_practical' does not exist"
        exit 1
    fi
    if ! branch_exists "next_cpp26"; then
        print_error "Branch 'next_cpp26' does not exist"
        exit 1
    fi
    print_success "Source branches exist"
    
    print_step "Checking for existing release branches..."
    if branch_exists "$PRACTICAL_BRANCH"; then
        print_warning "Branch '$PRACTICAL_BRANCH' already exists"
        read -p "Delete and recreate? (y/N): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            git branch -D "$PRACTICAL_BRANCH"
            print_success "Deleted old branch"
        else
            print_error "Aborted by user"
            exit 1
        fi
    fi
    
    if branch_exists "$CPP26_BRANCH"; then
        print_warning "Branch '$CPP26_BRANCH' already exists"
        read -p "Delete and recreate? (y/N): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            git branch -D "$CPP26_BRANCH"
            print_success "Deleted old branch"
        else
            print_error "Aborted by user"
            exit 1
        fi
    fi
    
    print_step "Checking for existing tags..."
    if tag_exists "$PRACTICAL_VERSION"; then
        print_warning "Tag '$PRACTICAL_VERSION' already exists"
        read -p "Delete and recreate? (y/N): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            git tag -d "$PRACTICAL_VERSION"
            print_success "Deleted old tag"
        else
            print_error "Aborted by user"
            exit 1
        fi
    fi
    
    if tag_exists "$CPP26_VERSION"; then
        print_warning "Tag '$CPP26_VERSION' already exists"
        read -p "Delete and recreate? (y/N): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            git tag -d "$CPP26_VERSION"
            print_success "Deleted old tag"
        else
            print_error "Aborted by user"
            exit 1
        fi
    fi
    
    print_success "Pre-flight checks passed"
}

# ==============================================================================
# Create Practical Release
# ==============================================================================

create_practical_release() {
    print_header "Creating v2.0-practical Release"
    
    # Save current branch
    ORIGINAL_BRANCH=$(git rev-parse --abbrev-ref HEAD)
    
    print_step "Switching to next_practical..."
    git checkout next_practical
    
    print_step "Creating orphan branch '$PRACTICAL_BRANCH'..."
    git checkout --orphan "$PRACTICAL_BRANCH"
    
    print_step "Staging all files..."
    git add -A
    
    print_step "Creating release commit..."
    git commit -m "Release v2.0-practical: Practical implementation with Boost.PFR

XOffsetDatastructure v2.0 - Practical Edition

This is a production-ready implementation using Boost.PFR for compile-time
type safety and reflection capabilities.

Features:
---------
• Type-safe offset-based containers (XVector, XMap, XSet, XString)
• Compile-time type safety checks using Boost.PFR
• Zero-copy binary serialization
• Memory-efficient growth strategy (1.1x factor)
• Cross-platform support (x86-64, ARM64)
• Automatic code generation from YAML schemas
• Comprehensive test suite

Platform Support:
-----------------
• Linux (x86-64, ARM64)
• macOS (x86-64, ARM64)
• Windows (x86-64)
• iOS (ARM64)
• Android (ARM64)

Requirements:
-------------
• C++20 compiler (Clang 15+, GCC 11+, MSVC 2022+)
• Boost.PFR (header-only)
• CMake 3.15+

Performance Characteristics:
----------------------------
• Sequential-fit allocator
• 8-byte alignment
• 10% growth factor (memory efficient)
• Offset-based pointers (relocatable buffers)

Type Safety:
------------
• Compile-time type signature validation
• Binary compatibility checks
• Protection against layout corruption
• Automatic size/offset verification

Documentation:
--------------
• See examples/helloworld.cpp for basic usage
• See examples/demo.cpp for comprehensive features
• See schemas/README.md for schema format
• Run tests with: cmake --build build --target test

License: MIT
"
    
    print_step "Creating tag '$PRACTICAL_VERSION'..."
    git tag -a "$PRACTICAL_VERSION" -m "XOffsetDatastructure v2.0 - Practical Edition

Production-ready implementation with Boost.PFR for type safety.

Key Features:
• Compile-time type safety using Boost.PFR reflection
• Zero-copy binary serialization
• Cross-platform support (Linux, macOS, Windows, iOS, Android)
• Memory-efficient offset-based containers
• Automatic code generation from YAML schemas

Requirements:
• C++20
• Boost.PFR (header-only)
• CMake 3.15+

Tested Compilers:
• Clang 15+
• GCC 11+
• MSVC 2022+
• Apple Clang 14+
"
    
    print_success "Practical release created successfully"
    
    # Return to original branch
    git checkout "$ORIGINAL_BRANCH"
}

# ==============================================================================
# Create C++26 Release
# ==============================================================================

create_cpp26_release() {
    print_header "Creating v2.0-cpp26 Release"
    
    # Save current branch
    ORIGINAL_BRANCH=$(git rev-parse --abbrev-ref HEAD)
    
    print_step "Switching to next_cpp26..."
    git checkout next_cpp26
    
    print_step "Creating orphan branch '$CPP26_BRANCH'..."
    git checkout --orphan "$CPP26_BRANCH"
    
    print_step "Staging all files..."
    git add -A
    
    print_step "Creating release commit..."
    git commit -m "Release v2.0-cpp26: C++26 reflection implementation

XOffsetDatastructure v2.0 - C++26 Edition

This is an experimental implementation using native C++26 reflection (P2996)
for compile-time type introspection without external dependencies.

Features:
---------
• Native C++26 reflection (P2996 - Reflection for C++26)
• Zero external dependencies (no Boost required)
• Compile-time type introspection and validation
• Advanced type signature system
• Member iteration and splicing
• Polymorphic type rejection
• Field limit validation

Reflection Capabilities:
------------------------
• Automatic struct member enumeration
• Compile-time type signature generation
• Member name and offset introspection
• Type compatibility validation
• Recursive type analysis

Type Safety:
------------
• Compile-time size/alignment checks
• Binary layout validation
• Virtual pointer detection
• Non-aggregate type rejection
• Field count validation (max 32 fields)

Requirements:
-------------
• C++26 (experimental)
• Clang with P2996 support (experimental build)
• CMake 3.15+

Building:
---------
Requires experimental Clang build with P2996 reflection support:
• See scripts/build_clang_p2996_wsl.sh for build instructions
• Or use pre-built experimental Clang toolchain

Performance:
------------
• Zero runtime overhead (all reflection at compile-time)
• No external library dependencies
• Optimized consteval functions
• Efficient type signature computation

Documentation:
--------------
• See tests/test_reflection_*.cpp for reflection examples
• See tests/test_type_introspection.cpp for advanced usage
• See tests/test_splice_operations.cpp for meta-programming

Status:
-------
⚠️  EXPERIMENTAL - Requires compiler with C++26 P2996 support
    Not recommended for production use until C++26 is standardized

License: MIT
"
    
    print_step "Creating tag '$CPP26_VERSION'..."
    git tag -a "$CPP26_VERSION" -m "XOffsetDatastructure v2.0 - C++26 Edition (Experimental)

Experimental implementation using native C++26 reflection (P2996).

Key Features:
• Native C++26 reflection without external dependencies
• Zero-overhead compile-time type introspection
• Advanced meta-programming capabilities
• Automatic type signature generation

Requirements:
• C++26 (experimental)
• Clang with P2996 reflection support
• CMake 3.15+

Status: EXPERIMENTAL
⚠️  This version requires an experimental compiler build.
    Not recommended for production use.

Build Instructions:
See scripts/build_clang_p2996_wsl.sh for compiler setup.
"
    
    print_success "C++26 release created successfully"
    
    # Return to original branch
    git checkout "$ORIGINAL_BRANCH"
}

# ==============================================================================
# Push to Remote
# ==============================================================================

push_to_remote() {
    print_header "Push to Remote Repository"
    
    print_warning "This will push the following to origin:"
    echo "  • Branch: $PRACTICAL_BRANCH"
    echo "  • Tag: $PRACTICAL_VERSION"
    echo "  • Branch: $CPP26_BRANCH"
    echo "  • Tag: $CPP26_VERSION"
    echo ""
    
    read -p "Continue? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        print_warning "Skipping remote push"
        return
    fi
    
    print_step "Pushing practical release..."
    git push origin "$PRACTICAL_BRANCH"
    git push origin "$PRACTICAL_VERSION"
    print_success "Practical release pushed"
    
    print_step "Pushing C++26 release..."
    git push origin "$CPP26_BRANCH"
    git push origin "$CPP26_VERSION"
    print_success "C++26 release pushed"
    
    print_success "All releases pushed to remote"
}

# ==============================================================================
# Summary
# ==============================================================================

print_summary() {
    print_header "Release Summary"
    
    echo -e "${GREEN}✓ Successfully created two independent releases:${NC}\n"
    
    echo -e "${BLUE}v2.0-practical${NC} (Production-ready)"
    echo "  Branch: $PRACTICAL_BRANCH"
    echo "  Tag:    $PRACTICAL_VERSION"
    echo "  Type:   Boost.PFR implementation"
    echo "  Status: Stable"
    echo ""
    
    echo -e "${BLUE}v2.0-cpp26${NC} (Experimental)"
    echo "  Branch: $CPP26_BRANCH"
    echo "  Tag:    $CPP26_VERSION"
    echo "  Type:   C++26 reflection (P2996)"
    echo "  Status: Experimental"
    echo ""
    
    echo -e "${YELLOW}Next Steps:${NC}"
    echo "  1. Verify releases locally:"
    echo "     git checkout $PRACTICAL_BRANCH"
    echo "     git checkout $CPP26_BRANCH"
    echo ""
    echo "  2. Create GitHub Releases at:"
    echo "     https://github.com/<your-username>/XOffsetDatastructure/releases/new"
    echo ""
    echo "  3. Recommended release notes:"
    echo "     • Mark v2.0-practical as 'Latest release'"
    echo "     • Mark v2.0-cpp26 as 'Pre-release'"
    echo ""
}

# ==============================================================================
# Main Execution
# ==============================================================================

main() {
    print_header "XOffsetDatastructure v2.0 Release"
    
    echo "This script will create two independent release branches:"
    echo "  • v2.0-practical: Production-ready with Boost.PFR"
    echo "  • v2.0-cpp26: Experimental with C++26 reflection"
    echo ""
    echo "Both releases will have clean commit history (single commit)."
    echo ""
    
    read -p "Continue? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        print_error "Aborted by user"
        exit 1
    fi
    
    preflight_check
    create_practical_release
    create_cpp26_release
    push_to_remote
    print_summary
    
    print_success "Release process completed!"
}

# Run main function
main "$@"
