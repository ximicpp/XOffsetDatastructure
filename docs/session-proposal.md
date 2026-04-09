# Session Proposal

## Title

From Hand-Written Allocator Constructors to Plain Structs: C++26 Reflection at the Control Point

## Abstract

Some high-performance serialization libraries get their speed by making `save()` and `load()` behave more like moving a memory buffer than encoding fields one by one. The cost shows up in user code: otherwise ordinary structs grow hand-written allocator constructors and move paths just so the library can build and relocate them correctly.

This session shows a practical C++26 reflection pattern for moving those type-dependent construction and migration decisions back into the library. In a real redesign of a zero-encoding serialization library, the key move was to reflect at the allocator's `construct()` boundary instead of depending on per-type construction boilerplate. The visible result is simpler user code: the same type goes from a hand-written allocator constructor to a plain struct. The deeper change is architectural: one reflection-driven member model now handles buffer-aware construction, transfer, and migration while staying compatible with existing allocator-aware types.

The talk stays centered on construction. Transfer and compaction appear only as brief follow-on examples that confirm the same reflection-driven mechanism can be reused without pushing migration logic back into user types. Attendees will leave with a reusable rule for modern C++ library design, plus the limits of the approach: toolchain maturity, portability constraints, and when a conventional serialization design is still the better fit.

## Format

60-minute session, adaptable to 30 minutes.

## Audience

Experienced C++ programmers, library authors, and engineers interested in C++26 reflection, memory layout, generic programming, and practical API design.

## What Attendees Will Learn

- Why zero-encoding serialization tends to leak construction and movement logic into user-defined types.
- How to use C++26 reflection at a library control point to centralize construction and migration decisions instead of depending on type-local customization.
- How one reflected member model can support construction, transfer, reallocation, and compaction.
- How to judge the tradeoffs: toolchain maturity, portability limits, and when reflection is not the right answer.

## Outline

### 1. The Before/After Problem

- What this style of serialization is and why `save()` / `load()` can approach copying a buffer.
- Why arena-resident containers and position-independent references tend to push construction and migration logic into user code.
- A concrete before/after sketch: from a hand-written allocator-aware aggregate to a plain zero-boilerplate struct.
- The key tension and preview of the core move: solve the problem at the allocator's decision point, not in every type.

### 2. The Short Workaround History

- Brief historical examples: generated constructors, aggregate-only reflection substitutes, and mirror-type maintenance.
- The architectural smell: multiple workarounds all compensating for the same missing capability.

### 3. Reflect at the Control Point

- The core design move: intercept allocator construction and centralize type-dependent decisions inside the library.
- Using C++26 reflection to inspect members, detect buffer-aware subobjects, and recurse into composites.
- Supporting existing allocator-aware types instead of breaking them.
- Why this design avoids much of the per-type registration and generation pressure older approaches created.

### 4. Brief Reuse Beyond Construction

- Why construction alone is not enough once containers move and reallocate.
- How the same reflected member model supports transfer without reintroducing type-local migration boilerplate.
- How compaction appears only as a brief second example that confirms the same approach still scales to nested types and deep object graphs.

### 5. Limits, Tradeoffs, and the General Rule

- Toolchain reality: C++26 reflection support is still emerging.
- Portability constraints of zero-encoding designs.
- Schema-evolution limitations versus traditional serialization systems.
- When this design is the right tool, and when it is not.
- The takeaway: reflect where the library must make a decision, not where users define their types.

## Why This Fits

- It uses a modern C++ language feature in a practical, non-toy design.
- It turns a concrete serialization case study into a reusable library design pattern.
- It is relevant to library design, serialization, generic programming, and software design.
- It is grounded in real engineering tradeoffs rather than novelty alone.
- It offers a pattern that many attendees can likely reuse in other library code that has to make type-dependent construction or migration decisions.
