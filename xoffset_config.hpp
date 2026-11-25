#pragma once
// Compiler and platform detection, the attribute macros and the build
// switches with their defaults: every XOffset header takes them from here.
// C++17; includes no other XOffset header.

// ---- Platform ---------------------------------------------------------------
// 64-bit little-endian only, with the scalar representation every supported ABI
// shares: xoffset_arena.hpp refuses anything else at compile time, and there is no
// switch that turns the check off.

#if defined(_WIN32)
// The lean <windows.h>, without min/max: no rpcndr.h (`small`, `hyper`), winsock.h or
// shellapi.h behind a client's back. Both macros are set only for this include; a
// client that needs more includes those headers itself (or <windows.h> first).
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#    define XOFFSET_DETAIL_UNDEF_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#    define XOFFSET_DETAIL_UNDEF_NOMINMAX
#  endif
#  include <windows.h>
#  ifdef XOFFSET_DETAIL_UNDEF_LEAN_AND_MEAN
#    undef WIN32_LEAN_AND_MEAN
#    undef XOFFSET_DETAIL_UNDEF_LEAN_AND_MEAN
#  endif
#  ifdef XOFFSET_DETAIL_UNDEF_NOMINMAX
#    undef NOMINMAX
#    undef XOFFSET_DETAIL_UNDEF_NOMINMAX
#  endif
#endif

// ---- Exceptions ---------------------------------------------------------------
// On (the default when the compiler has them): every failure throws; the try_*
// receive entries catch and return the same code and message. Off
// (-fno-exceptions, MSVC /EHs-c-, or -DXOFFSET_NO_EXCEPTIONS=1): the receive
// path returns its first failure up to the try_* entry, and any other failure
// calls the fatal handler.
#if !defined(XOFFSET_NO_EXCEPTIONS)
#  if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#    define XOFFSET_NO_EXCEPTIONS 0
#  else
#    define XOFFSET_NO_EXCEPTIONS 1
#  endif
#endif
#if !XOFFSET_NO_EXCEPTIONS && !(defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND))
#  error "XOFFSET_NO_EXCEPTIONS=0 requires a build with exceptions enabled"
#endif

// ---- Attributes ---------------------------------------------------------------
// Forced inlining, and its opposite. COLD also tells GCC and Clang that the
// call is rare (MSVC has only noinline).
#if defined(_MSC_VER) && !defined(__clang__)
#define XOFFSET_DETAIL_INLINE __forceinline
#define XOFFSET_DETAIL_NOINLINE __declspec(noinline)
#define XOFFSET_DETAIL_COLD __declspec(noinline)
#define XOFFSET_DETAIL_LAMBDA_INLINE [[msvc::forceinline]]
#elif defined(__GNUC__) || defined(__clang__)
#define XOFFSET_DETAIL_INLINE inline __attribute__((always_inline))
#define XOFFSET_DETAIL_NOINLINE __attribute__((noinline))
#define XOFFSET_DETAIL_COLD __attribute__((noinline, cold))
// A lambda's call operator inlined (written after its parameter list).
#define XOFFSET_DETAIL_LAMBDA_INLINE __attribute__((always_inline))
#else
#define XOFFSET_DETAIL_INLINE inline
#define XOFFSET_DETAIL_NOINLINE
#define XOFFSET_DETAIL_COLD
#define XOFFSET_DETAIL_LAMBDA_INLINE
#endif
// A value the optimizer must take as it is, in one register: a pointer that
// several loads use stays one address instead of being selected again for
// each (LLVM folds an offset into a select of a string's two buffers).
#if defined(__GNUC__) || defined(__clang__)
#define XOFFSET_DETAIL_OPAQUE(value) __asm__("" : "+r"(value))
#else
#define XOFFSET_DETAIL_OPAQUE(value) ((void)0)
#endif

// Inlining that GCC 16 does on its own in C++26 mode but not in C++23 (it
// keeps these out of line in push_back: +7% instructions on reloc.hero
// push_back_grow). Forced only for GCC below C++26: C++26 machine code is
// unchanged, and Clang inlines less around them when they are forced.
#if __cplusplus <= 202302L && defined(__GNUC__) && !defined(__clang__)
#define XOFFSET_DETAIL_INLINE_GCC_BELOW_26 XOFFSET_DETAIL_INLINE
#else
#define XOFFSET_DETAIL_INLINE_GCC_BELOW_26
#endif

// Forced inlining for Clang only, where its inline cost model stops at a
// function that GCC still inlines (the flat map and set erase).
#if defined(__clang__)
#define XOFFSET_DETAIL_INLINE_CLANG XOFFSET_DETAIL_INLINE
#else
#define XOFFSET_DETAIL_INLINE_CLANG inline
#endif

// Kept out of line for Clang only, where inlining a function can make Clang
// keep its caller out of line in turn (assign_trivial_copy); GCC inlines both.
#if defined(__clang__)
#define XOFFSET_DETAIL_NOINLINE_CLANG XOFFSET_DETAIL_NOINLINE
#else
#define XOFFSET_DETAIL_NOINLINE_CLANG inline
#endif

// Forced inlining for MSVC only, where its inline cost model stops at flat-map
// insert/erase helpers that GCC and Clang already inline.
#if defined(_MSC_VER) && !defined(__clang__)
#define XOFFSET_DETAIL_INLINE_MSVC XOFFSET_DETAIL_INLINE
#else
#define XOFFSET_DETAIL_INLINE_MSVC inline
#endif

// Forced inlining only without exceptions (the inline-string check of the
// wire validation; see validate_wire_element).
#if XOFFSET_NO_EXCEPTIONS
#define XOFFSET_DETAIL_INLINE_NO_EXCEPTIONS XOFFSET_DETAIL_INLINE
#else
#define XOFFSET_DETAIL_INLINE_NO_EXCEPTIONS inline
#endif

// A cache prefetch hint for reading (rw 0) or writing (rw 1). PREFETCH is
// emitted by GCC and Clang only; PREFETCH_WITH_MSVC also by MSVC on x64 (the
// delta and receiver loops).
#if defined(__GNUC__) || defined(__clang__)
#define XOFFSET_DETAIL_PREFETCH(p, rw) __builtin_prefetch((p), (rw))
#define XOFFSET_DETAIL_PREFETCH_WITH_MSVC(p, rw) __builtin_prefetch((p), (rw))
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_AMD64))
#include <intrin.h>
#define XOFFSET_DETAIL_PREFETCH(p, rw) ((void)(p))
#define XOFFSET_DETAIL_PREFETCH_WITH_MSVC(p, rw) \
    ((rw) ? _m_prefetchw(const_cast<void*>(static_cast<const void*>(p))) \
          : _mm_prefetch(reinterpret_cast<const char*>(p), _MM_HINT_T0))
#else
#define XOFFSET_DETAIL_PREFETCH(p, rw) ((void)(p))
#define XOFFSET_DETAIL_PREFETCH_WITH_MSVC(p, rw) ((void)(p))
#endif

// An assumption the optimizer may use.
#if defined(_MSC_VER) && !defined(__clang__)
#define XOFFSET_DETAIL_ASSUME(x) __assume(x)
#elif defined(__clang__)
#define XOFFSET_DETAIL_ASSUME(x) __builtin_assume(x)
#elif defined(__GNUC__)
// Not [[assume]]: GCC outlines its expression and passes the address of the
// enclosing object, which keeps a proxy in memory (dead stores every op).
#define XOFFSET_DETAIL_ASSUME(x) do { if (!(x)) __builtin_unreachable(); } while (0)
#else
#define XOFFSET_DETAIL_ASSUME(x) ((void)0)
#endif

#if defined(__cplusplus) && __cplusplus >= 202002L
#  define XOFFSET_DETAIL_UNLIKELY [[unlikely]]
#else
#  define XOFFSET_DETAIL_UNLIKELY
#endif

// A loop that usually runs one to three times: no vector code (its prologue
// and epilogue cost more than the loop).
#if !defined(XOFFSET_DETAIL_NOVECTOR)
#  if defined(__clang__)
#    define XOFFSET_DETAIL_NOVECTOR _Pragma("clang loop vectorize(disable)")
#  elif defined(__GNUC__) && __GNUC__ >= 14
#    define XOFFSET_DETAIL_NOVECTOR _Pragma("GCC novector")
#  elif defined(_MSC_VER)
#    define XOFFSET_DETAIL_NOVECTOR __pragma(loop(no_vector))
#  else
#    define XOFFSET_DETAIL_NOVECTOR
#  endif
#endif

// [[indeterminate]] (C++26) where the compiler has it: an automatic variable
// whose bytes are written before they are read does not get the erroneous-
// behaviour fill (GCC 16 zeroes it; Clang without the attribute does not).
#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(indeterminate)
#define XOFFSET_INDETERMINATE [[indeterminate]]
#endif
#endif
#ifndef XOFFSET_INDETERMINATE
#define XOFFSET_INDETERMINATE
#endif

// ---- Hardening switches (docs/V2_THREAT_MODEL.md, section 4.1) -----------
// The measures below cost time on the mutate and send paths and guard only
// against stale bytes on the wire (threat T5) or against a caller's own
// mistake, so each is OFF unless a build asks for it: -DXOFFSET_<NAME>=1 for
// one, -DXOFFSET_HARDENED=1 for every one the build does not set itself.
// They change inline functions: define them the same way in every
// translation unit of a program.
// save_compact writes no holes and zeroes container slack and string tails,
// with or without them; it copies the implicit padding of records as it is
// (docs/V2_THREAT_MODEL.md, T5).
//   XOFFSET_SCRUB_POPPED     M4: pop_back() zeroes the removed element.
//   XOFFSET_SCRUB_ERASED     M4: clear(), a shrinking resize(), erase and a
//                            shorter assignment zero the bytes they vacate.
//   XOFFSET_SCRUB_PADDING    M6: implicit padding inside stored elements is
//                            zeroed when the library constructs or copies them.
//   XOFFSET_PIN_QUARANTINE   a block retired inside an edit() / read() scope
//                            is not reused before the scope closes, so a plain
//                            reference the operation invalidated still reads
//                            its old bytes.
//   XOFFSET_SAVE_SELF_CHECK  save_verified(_into) re-checks the arena header
//                            and that the output does not overlap the source.
//   XOFFSET_SCRUB_RESET      reset() zeroes the payload it discards. Off: the
//                            old bytes stay past the new used_end and the next
//                            content's blocks start with them (a block's slack
//                            and padding then carry the previous content).
// Not a switch, because deltas rely on it (a hole's bytes are zero on both
// sides): a retired block is always zeroed (M3).
//
// A precondition check, off by default for the same reason, on in builds
// without NDEBUG as well:
//   XOFFSET_CHECK_SORTED     XMap / XSet assign_sorted() check that their
//                            input is strictly ascending and throw
//                            XErrc::invalid_argument, the container unchanged,
//                            when it is not. Off: an unsorted input gives a
//                            container whose lookups miss.
//
// One check is a switch that is ON by default, because without
// it any client can make a server read or write outside the wire:
//   XOFFSET_CHECK_UPLINK     XReceiver<XLink::from_client> checks by default
//                            (bounds for view(), full for a mutable result)
//                            and refuses XCheck::admit. -DXOFFSET_CHECK_UPLINK=0,
//                            for a deployment whose clients are its own code
//                            over a protected transport: a client link
//                            defaults to XCheck::admit like the trusted links;
//                            XCheck::bounds and XCheck::full remain per call.
//                            XOFFSET_HARDENED does not change it.
#ifndef XOFFSET_HARDENED
#  define XOFFSET_HARDENED 0
#endif
#ifndef XOFFSET_SCRUB_POPPED
#  define XOFFSET_SCRUB_POPPED XOFFSET_HARDENED
#endif
#ifndef XOFFSET_SCRUB_ERASED
#  define XOFFSET_SCRUB_ERASED XOFFSET_HARDENED
#endif
#ifndef XOFFSET_SCRUB_PADDING
#  define XOFFSET_SCRUB_PADDING XOFFSET_HARDENED
#endif
#ifndef XOFFSET_PIN_QUARANTINE
#  define XOFFSET_PIN_QUARANTINE XOFFSET_HARDENED
#endif
#ifndef XOFFSET_SAVE_SELF_CHECK
#  define XOFFSET_SAVE_SELF_CHECK XOFFSET_HARDENED
#endif
#ifndef XOFFSET_SCRUB_RESET
#  define XOFFSET_SCRUB_RESET XOFFSET_HARDENED
#endif
#ifndef XOFFSET_CHECK_UPLINK
#  define XOFFSET_CHECK_UPLINK 1
#endif
#ifndef XOFFSET_CHECK_SORTED
#  if XOFFSET_HARDENED || !defined(NDEBUG)
#    define XOFFSET_CHECK_SORTED 1
#  else
#    define XOFFSET_CHECK_SORTED 0
#  endif
#endif

// ---- Debug checks -------------------------------------------------------------
// On by default in builds without NDEBUG.
//   XOFFSET_SYNC_CHECKS           the shared publisher's internal consistency checks.
//   XOFFSET_MOVE_OWNERSHIP_CHECKS a move assignment that steals a container's
//                                 tree first excludes that either head lies
//                                 inside the other's tree; release builds rely
//                                 on the caller not moving a container into
//                                 (or out of) its own subobject.
#if !defined(XOFFSET_SYNC_CHECKS)
#  if defined(NDEBUG)
#    define XOFFSET_SYNC_CHECKS 0
#  else
#    define XOFFSET_SYNC_CHECKS 1
#  endif
#endif
#if !defined(XOFFSET_MOVE_OWNERSHIP_CHECKS)
#  if defined(NDEBUG)
#    define XOFFSET_MOVE_OWNERSHIP_CHECKS 0
#  else
#    define XOFFSET_MOVE_OWNERSHIP_CHECKS 1
#  endif
#endif
// XReceiver: every admit runs the full check instead (a sender bug is then
// rejected rather than used). Also on under AddressSanitizer or ThreadSanitizer.
#ifndef XOFFSET_FORCE_FULL_CHECK
#  if !defined(NDEBUG) || defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
#    define XOFFSET_FORCE_FULL_CHECK 1
#  elif defined(__has_feature)
#    if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
#      define XOFFSET_FORCE_FULL_CHECK 1
#    endif
#  endif
#  ifndef XOFFSET_FORCE_FULL_CHECK
#    define XOFFSET_FORCE_FULL_CHECK 0
#  endif
#endif

// ---- Per-thread caches -----------------------------------------------------
// Per-thread caches (this slab's free lists and the owner cache in
// xoffsetdatastructure.hpp). Each keeps a trivial thread_local
// flag that its nontrivial cache clears on thread exit, so late destructors
// take the global path. GCC on MinGW and Cygwin emulates TLS (emutls): a
// thread's TLS blocks can be freed before its thread_local destructors run,
// and a TLS access from such a destructor gets a fresh block, so neither the
// cache nor the flag is reliable there. Those targets get no per-thread
// caches; every path uses the global structures. Clang on MinGW uses native
// TLS and keeps them.
#ifndef XOFFSET_THREAD_CACHES
#  if (defined(__MINGW32__) || defined(__CYGWIN__)) && defined(__GNUC__) && !defined(__clang__)
#    define XOFFSET_THREAD_CACHES 0
#  else
#    define XOFFSET_THREAD_CACHES 1
#  endif
#endif

// ---- Validation scratch memory resource (optional std::pmr) ----------------
// Wire verification only needs a polymorphic allocate/deallocate hook when a
// payload exceeds the 32 KiB inline granule bitmap and no retained scratch is
// passed. Apple's system libc++ exports std::pmr symbols only from macOS 14 /
// iOS 17 / tvOS 17 / watchOS 10; on older Apple deployment targets (or when
// -DXOFFSET_USE_PMR=0 is set), XOffset uses a header-only XMemoryResource
// backed by global operator new/delete with no <memory_resource> dependency.
#if defined(__APPLE__)
#  include <version>
#  if defined(_LIBCPP_HAS_VENDOR_AVAILABILITY_ANNOTATIONS)
#    define XOFFSET_DETAIL_APPLE_SYSTEM_LIBCXX _LIBCPP_HAS_VENDOR_AVAILABILITY_ANNOTATIONS
#  elif defined(_LIBCPP_VERSION) && !defined(_LIBCPP_HAS_NO_VENDOR_AVAILABILITY_ANNOTATIONS)
#    define XOFFSET_DETAIL_APPLE_SYSTEM_LIBCXX 1
#  else
#    define XOFFSET_DETAIL_APPLE_SYSTEM_LIBCXX 0
#  endif
#else
#  define XOFFSET_DETAIL_APPLE_SYSTEM_LIBCXX 0
#endif

#if XOFFSET_DETAIL_APPLE_SYSTEM_LIBCXX && ( \
    (defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__) && __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 140000) || \
    (defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) && __ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__ < 170000) || \
    (defined(__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__) && __ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__ < 170000) || \
    (defined(__ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__) && __ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__ < 100000))
#  define XOFFSET_DETAIL_APPLE_PMR_AVAILABLE 0
#else
#  define XOFFSET_DETAIL_APPLE_PMR_AVAILABLE 1
#endif

#if !defined(XOFFSET_USE_PMR)
#  if !XOFFSET_DETAIL_APPLE_PMR_AVAILABLE
#    define XOFFSET_USE_PMR 0
#  elif defined(__has_include)
#    if __has_include(<memory_resource>)
#      define XOFFSET_USE_PMR 1
#    else
#      define XOFFSET_USE_PMR 0
#    endif
#  else
#    define XOFFSET_USE_PMR 1
#  endif
#endif

#if XOFFSET_USE_PMR && !XOFFSET_DETAIL_APPLE_PMR_AVAILABLE
#  error "XOFFSET_USE_PMR=1 needs macOS 14 / iOS 17 / tvOS 17 / watchOS 10 or newer (std::pmr in the Apple system libc++); omit XOFFSET_USE_PMR or set -DXOFFSET_USE_PMR=0 for older Apple targets"
#endif

// Floating-point std::to_chars is exported by Apple's system libc++ starting
// from macOS 13.3 / iOS 16.3 / tvOS 16.3 / watchOS 9.4.
#if XOFFSET_DETAIL_APPLE_SYSTEM_LIBCXX && ( \
    (defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__) && __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 130300) || \
    (defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) && __ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__ < 160300) || \
    (defined(__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__) && __ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__ < 160300) || \
    (defined(__ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__) && __ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__ < 90400))
#  define XOFFSET_DETAIL_TO_CHARS_FLOAT 0
#else
#  define XOFFSET_DETAIL_TO_CHARS_FLOAT 1
#endif

// The switches as constants: code tests them with if constexpr, so each
// switch has one #if (its definition above).
namespace XOffsetDatastructure::detail {
inline constexpr bool scrub_popped = XOFFSET_SCRUB_POPPED != 0;
inline constexpr bool scrub_erased = XOFFSET_SCRUB_ERASED != 0;
inline constexpr bool scrub_reset = XOFFSET_SCRUB_RESET != 0;
inline constexpr bool pin_quarantine = XOFFSET_PIN_QUARANTINE != 0;
inline constexpr bool save_self_check = XOFFSET_SAVE_SELF_CHECK != 0;
inline constexpr bool check_sorted = XOFFSET_CHECK_SORTED != 0;
} // namespace XOffsetDatastructure::detail
