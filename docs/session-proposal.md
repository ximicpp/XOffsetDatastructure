# Session Proposal

## Title

Reflect at the Control Point: Reducing User-Type Boilerplate with C++26 Reflection

## Abstract

Zero-encoding save/restore systems — systems that save and restore whole in-memory buffers instead of encoding fields one by one — get their speed by skipping per-field processing entirely. That speed often comes at a cost in user-defined types: types that should only describe data end up carrying allocator constructors and relocation-aware construction paths.

This session shows a practical C++26 reflection pattern for moving type-dependent construction and migration logic into the library. In a zero-encoding serialization library, the key move is to reflect at the allocator's `construct()` boundary, where the library already has to decide how an object should be built. From that control point, the library can initialize allocator-aware members, recurse into composites, and keep existing allocator-aware types working. The visible result is less type-local boilerplate: many aggregate types can remain zero-boilerplate, while older allocator-aware types continue to work.

The main story is construction. Transfer during reallocation appears only as a short follow-on example, with compaction mentioned as a brief second example that shows the same member-level reflection strategy extends beyond a single path. Attendees will leave with a practical design rule for C++ libraries: put reflection at the point where the library must make a type-dependent decision, and keep the limits in view: toolchain maturity and where this design is not the right fit.

## Format

60-minute session, adaptable to 30 minutes.

## Audience

Experienced C++ programmers, library authors, and engineers interested in C++26 reflection, library design, memory layout, and practical generic programming.

## What Attendees Will Learn

- Why zero-encoding save/restore designs tend to leak buffer management, construction, and relocation concerns into user-defined types.
- How allocator `construct()` becomes the practical control point for reflection-driven construction and transfer.
- How one member-level reflection strategy can support construction, transfer during reallocation, and compaction.
- How to judge the limits: toolchain maturity and when not to use this design.

## Outline

### 1. Why Zero-Encoding Pushes Logic into Types

- What zero-encoding means in practice: `save()` / `load()` works by saving and restoring whole buffers rather than by encoding fields one by one.
- Why arena-resident containers and position-independent references force buffer-aware construction and relocation logic into user code.
- A concrete before/after sketch: a zero-boilerplate aggregate beside a legacy allocator-aware type.

### 2. Reflect at the Control Point

- The key move: intercept allocator construction at the point where the library already has to decide how an object should be built.
- Use C++26 reflection to inspect members, inject allocators, recurse into composites, and keep legacy allocator-aware types working.
- The architectural shift: type-dependent construction and migration logic moves out of user types and into one library-owned mechanism.

### 3. A Brief Look Beyond Construction

- Why construction alone is not enough once containers move and reallocate.
- Show how the same member-level reflection strategy also covers transfer during reallocation, with compaction kept to a short second example.
- Why the same approach still works for nested types and deeper object graphs.

### 4. Limits and Takeaway

- Toolchain reality: P2996 is available in GCC trunk and an experimental Clang fork, but not yet in all major toolchains.
- General rule: reflect where the library must make a type-dependent decision, not where users define their types.

## Why This Fits

- It applies a modern C++ language feature to a concrete library design problem rather than a toy example.
- It turns a concrete serialization case study into a reusable pattern for type-dependent construction and migration, without depending on schema files and generated types as the primary path.
- It is relevant to library design, serialization, generic programming, and software design.
- It includes real constraints and non-goals, not just the happy path.
