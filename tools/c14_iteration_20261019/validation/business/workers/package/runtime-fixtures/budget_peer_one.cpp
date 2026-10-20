#include "xoffset_snapshot_index_budget.hpp"
#if defined(_WIN32)
#define PEER_EXPORT __declspec(dllexport)
#else
#define PEER_EXPORT __attribute__((visibility("default")))
#endif
namespace si = XOffsetDatastructure::snapshot_index_detail;
extern "C" PEER_EXPORT void* budget_one_registry() { return si::registry(); }
extern "C" PEER_EXPORT bool budget_one_reserve(const void* key, std::size_t bytes) { return si::reserve(key, bytes); }
extern "C" PEER_EXPORT void budget_one_release(const void* key, std::size_t bytes) { si::release(key, bytes); }
extern "C" PEER_EXPORT std::size_t budget_one_bytes() { return si::process_requested_bytes(); }
