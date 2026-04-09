# Session Proposal

## Title

How C++26 Reflection Removes Boilerplate from Zero-Encoding Serialization

## Abstract

C++26 reflection can remove an entire class of boilerplate from performance-critical libraries. In zero-encoding serialization, `save()` and `load()` can get close to moving raw bytes, but the price is often allocator plumbing, custom construction paths, and careful move logic inside user-defined types.

This session shows how C++26 static reflection can move that work out of user types and into library control points. In a real serialization library redesign, the key move was to reflect at the allocator's `construct()` boundary instead of generating constructors per type. That single decision point let the library inspect members, inject allocators where needed, recurse into composites, and remain compatible with existing allocator-aware types.

The main narrative focuses on construction, with transfer as the natural follow-up and compaction as a brief second use case rather than a full design tour. Attendees will leave with a practical design rule they can reuse beyond serialization: reflect where the library must decide what to do, not where users define their types. The session also covers the limits of the approach, including toolchain maturity, portability constraints, and when a conventional serialization design is the better choice.

## Format

60-minute session, adaptable to 30 minutes.

## Audience

Experienced C++ programmers, library authors, and engineers interested in C++26 reflection, memory layout, generic programming, and practical API design.

## What Attendees Will Learn

- Why zero-encoding serialization tends to leak allocator and construction complexity into user-defined types.
- How to use C++26 reflection at a library control point instead of requiring per-type boilerplate, macros, or code generation.
- How one reflected member model can support construction, transfer, reallocation, and compaction.
- How to judge the tradeoffs: toolchain maturity, portability limits, and when reflection is not the right answer.

## Outline

### 1. Why Zero-Encoding Creates Boilerplate

- What zero-encoding serialization is and why it is attractive.
- Why arena-resident containers and position-independent references are required.
- Why these requirements usually force allocator constructors and custom move paths into user code.
- The key tension: excellent runtime behavior, poor type authoring experience.
- Preview of the core move: solve the problem at the allocator's decision point, not in every type.

### 2. The Workaround Stack Before Reflection

- Generated constructors and schema-driven code generation.
- Aggregate-only reflection substitutes and mirror-type maintenance.
- The architectural smell: multiple workarounds for the same missing capability.

### 3. Reflect at the Control Point

- The core design move: intercept allocator construction instead of generating type-local constructors.
- Using C++26 reflection to inspect members, detect buffer-aware subobjects, and recurse into composites.
- Supporting existing allocator-aware types instead of breaking them.
- Why this design composes better than registration-heavy or generator-heavy approaches.

### 4. One Member Model, Several Jobs

- Why construction alone is not enough once containers move and reallocate.
- How the same reflected member model supports transfer without reintroducing type-local boilerplate.
- How the strategy extends to compaction as a brief second case study.
- How one reflection model still scales to nested types and deep object graphs.

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
- It offers a reusable pattern that attendees can apply outside serialization.
