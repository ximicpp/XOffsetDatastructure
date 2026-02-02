# Design: CI/CD with Docker Support

## Context
XOffsetDatastructure requires Bloomberg's Clang P2996 compiler with C++26 reflection support. This specialized compiler:
- Is not available in standard package managers
- Requires 1-3 hours to build from source
- Needs specific configuration flags (`-freflection`, `-fexpansion-statements`, `-stdlib=libc++`)
- Requires libc++ runtime from the same build

Standard CI platforms (GitHub Actions, Travis, etc.) don't provide pre-built P2996 Clang images, necessitating a custom Docker solution.

## Goals
- Provide automated CI/CD for every push and pull request
- Create reproducible build environment for local development
- Minimize CI build time through Docker layer caching
- Support both WSL and Docker workflows for developers
- Ensure all 18 tests (6 basic + 12 reflection) pass consistently

## Non-Goals
- Supporting non-Clang compilers (project explicitly requires P2996)
- Building Clang P2996 on every CI run (too slow, use pre-built Docker image)
- Multi-platform CI (focus on Linux x86_64; macOS/Windows are secondary)
- Publishing Docker images to registry initially (can be added later)

## Decisions

### Docker Multi-Stage Build Strategy
**Decision:** Use multi-stage Dockerfile with separate build and runtime stages.

**Stages:**
1. **Builder stage** (`clang-builder`):
   - Base: `ubuntu:22.04` or similar
   - Clone and build Clang P2996 from Bloomberg's repository
   - Build with Ninja for speed
   - Install to `/opt/clang-p2996`
   - **Size:** ~10-15 GB

2. **Runtime stage** (`xoffset-dev`):
   - Base: `ubuntu:22.04`
   - Copy only `/opt/clang-p2996` from builder
   - Install minimal runtime dependencies (git, cmake, ninja, build-essential)
   - Add project source via COPY or volume mount
   - **Size:** ~3-5 GB

**Rationale:**
- Separates one-time expensive build from frequent project builds
- Runtime image is much smaller (faster to pull/push)
- Can cache builder stage indefinitely
- Allows local developers to skip Clang build if using pre-built image

**Alternatives Considered:**
- Single-stage build: Too slow, creates huge images
- Install from pre-built binary: No official P2996 binaries available
- Use GitHub Actions cache: Not suitable for multi-GB artifacts

### GitHub Actions Workflow Design
**Decision:** Build Docker image once, use for all test jobs.

**Workflow structure:**
```yaml
jobs:
  build-docker:
    - Build or pull cached Docker image
    - Push to GitHub Container Registry (ghcr.io) if needed
  
  test:
    needs: build-docker
    strategy:
      matrix:
        build-type: [Release, Debug]
    - Pull Docker image
    - Run build.sh with appropriate flags
    - Upload test results
```

**Caching Strategy:**
- Use GitHub Actions cache for Docker layers
- Cache key based on Dockerfile and Clang commit hash
- Rebuild only when Dockerfile or Clang version changes (rare)

**Rationale:**
- Separates concerns (image build vs project build)
- Allows parallel test jobs
- Efficient caching reduces CI time from hours to minutes

### Environment Variables
```dockerfile
ENV CC=/opt/clang-p2996/bin/clang
ENV CXX=/opt/clang-p2996/bin/clang++
ENV PATH=/opt/clang-p2996/bin:$PATH
ENV LD_LIBRARY_PATH=/opt/clang-p2996/lib:$LD_LIBRARY_PATH
```

**Rationale:** Ensures consistent compiler usage, matches build.sh expectations.

### Clang P2996 Build Configuration
```cmake
-DCMAKE_BUILD_TYPE=Release
-DLLVM_ENABLE_PROJECTS='clang'
-DLLVM_ENABLE_RUNTIMES='libcxx;libcxxabi;libunwind'
-DLLVM_TARGETS_TO_BUILD='X86'
-DLLVM_ENABLE_ASSERTIONS=OFF
-DLLVM_OPTIMIZED_TABLEGEN=ON
-DLIBCXX_ENABLE_EXPERIMENTAL_LIBRARY=ON
```

**Rationale:** Matches `scripts/build_clang_p2996_wsl.sh`, proven configuration.

## Risks / Trade-offs

### Risk: Long Initial Docker Build Time
- **Risk:** Building Clang P2996 takes 1-3 hours, may timeout on CI.
- **Mitigation:** 
  - Use multi-stage build with aggressive caching
  - Consider pre-building image weekly via scheduled workflow
  - Set timeout to 4 hours for build-docker job
  - Document that first build is slow

### Risk: Large Docker Image Size
- **Risk:** 3-5 GB runtime image is large, slow to pull.
- **Mitigation:**
  - Use Docker layer caching
  - Consider publishing to ghcr.io for faster pulls
  - Multi-stage build keeps size minimal
- **Trade-off:** Acceptable given specialized compiler requirements

### Risk: Docker Layer Cache Invalidation
- **Risk:** Small changes may invalidate cache, triggering full rebuild.
- **Mitigation:**
  - Order Dockerfile layers carefully (stable -> volatile)
  - Pin Clang commit hash in Dockerfile
  - Use `COPY` only for final project files

### Risk: Local Development Friction
- **Risk:** Developers may prefer WSL over Docker.
- **Mitigation:**
  - Keep both workflows documented
  - Make Docker optional, not mandatory
  - Provide helper scripts for common Docker operations

## Migration Plan

### Phase 1: Docker Infrastructure (Week 1)
1. Create Dockerfile and test local build
2. Validate all 18 tests pass in Docker
3. Document Docker usage

### Phase 2: GitHub Actions (Week 1)
1. Create basic CI workflow
2. Test on feature branch
3. Optimize caching strategy
4. Merge to main

### Phase 3: Optimization (Week 2)
1. Add pre-built image publishing
2. Add parallel test jobs
3. Performance tuning

### Rollback Plan
If CI fails critically:
- Disable workflow via GitHub UI (no code changes needed)
- Docker changes don't affect existing WSL workflow
- Can revert `.github/workflows/` directory

## Open Questions
1. **Image hosting:** Use GitHub Container Registry (ghcr.io) or Docker Hub?
   - **Recommendation:** ghcr.io (integrated, free for public repos)

2. **Clang update frequency:** How often to rebuild with latest P2996 commits?
   - **Recommendation:** Monthly or when bugs reported, not automatic

3. **Test parallelization:** Run all 18 tests in parallel or sequentially?
   - **Recommendation:** Sequential initially (simpler), parallelize if needed

4. **Windows support:** Should we add Windows Docker support?
   - **Recommendation:** No, WSL is sufficient for Windows users
