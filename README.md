### Introduction
XOffsetDatastructure is a serialization library designed to reduce or even eliminate the performance consumption of serialization and deserialization by utilizing zero-encoding and zero-decoding. It is also a collection of high-performance data structures designed for efficient read and in-place/non-in-place write, with performance comparable to STL.

### Release Branches

This repository maintains two parallel implementations:

#### [release/v2.0-practical](https://github.com/ximicpp/XOffsetDatastructure/tree/release/v2.0-practical) - **Recommended**
- **Reflection**: Uses [Boost.PFR](https://github.com/boostorg/pfr) for compile-time reflection
- **Compatibility**: C++20 and above
- **Stability**: Production-ready, thoroughly tested
- **Use Case**: Recommended for production environments

#### [release/v2.0-cpp26](https://github.com/ximicpp/XOffsetDatastructure/tree/release/v2.0-cpp26) - Experimental
- **Reflection**: Uses C++26 native reflection (`std::meta`)
- **Compatibility**: Requires C++26 (experimental compiler support)
- **Stability**: Experimental, cutting-edge technology
- **Use Case**: For exploring future C++ capabilities

> **Which version should I use?**  
> For most users, we recommend **release/v2.0-practical**. It provides all features with proven stability and broad compiler support.

### CppCon 2024
[CppCon 2024: Using Modern C++ to Build XOffsetDatastructure: A Zero-Encoding and Zero-Decoding High-Performance Serialization Library](https://github.com/CppCon/CppCon2024/blob/main/Presentations/Using_Modern_Cpp_to_Build_XOffsetDatastructure.pdf)

### CppCon 2025
[CppCon 2025: Cross-platform XOffsetDatastructure: Ensuring Zero-encoding/Zero-decoding Serialization Compatibility Through Compile-time Type Signatures](https://github.com/ximicpp/XOffsetDatastructure/blob/main/docs/Compile-timeTypeSignatures.pdf)

### PS:
Benchmark code and results: see the tag [“v1.0.0: CppCon 2024 Milestone Release (Latest)”](https://github.com/ximicpp/XOffsetDatastructure/releases/tag/v1.0.0)