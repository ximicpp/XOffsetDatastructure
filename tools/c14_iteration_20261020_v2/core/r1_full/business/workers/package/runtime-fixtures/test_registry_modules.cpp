#include "registry_module_api.hpp"
#include "validation_scratch.hpp"
#include <stdexcept>
#include <string>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {
void need(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

class Module {
#if defined(_WIN32)
    HMODULE handle_ = nullptr;
#else
    void* handle_ = nullptr;
#endif
    const ScratchRegistryProbeApi* api_ = nullptr;
    void close() noexcept {
        // Every context dies BEFORE unloading its module, whose functions and
        // context type may not be called after close. Central runtime stays live
        // because the executable itself links it.
        if (api_) api_->clear_context();
        api_ = nullptr;
        if (!handle_) return;
#if defined(_WIN32)
        ::FreeLibrary(handle_);
#else
        ::dlclose(handle_);
#endif
        handle_ = nullptr;
    }
public:
    explicit Module(const char* filename) {
        // CTest supplies ASCII target basenames in the target directory; this
        // avoids a Windows narrow-encoding conversion of a Unicode build path.
        const auto relative = std::string("./") + filename;
#if defined(_WIN32)
        handle_ = ::LoadLibraryA(relative.c_str());
        if (!handle_) throw std::runtime_error("probe LoadLibrary failed");
        auto entry = reinterpret_cast<ScratchRegistryProbeEntry>(
            ::GetProcAddress(handle_, "validation_scratch_probe"));
#else
        handle_ = ::dlopen(relative.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!handle_) throw std::runtime_error("probe dlopen failed");
        auto entry = reinterpret_cast<ScratchRegistryProbeEntry>(
            ::dlsym(handle_, "validation_scratch_probe"));
#endif
        if (entry) api_ = entry();
        if (!api_ || api_->version != 1) {
            close();
            throw std::runtime_error("probe entry/version invalid");
        }
    }
    ~Module() { close(); }
    Module(const Module&) = delete;
    Module& operator=(const Module&) = delete;
    const ScratchRegistryProbeApi& api() const noexcept { return *api_; }
};
}

int main(int argc, char** argv) {
    need(argc == 3, "need two probe module basenames");
    using xoffset_scratch::Limits;
    using xoffset_scratch::ValidationScratch;
    {
        Module a(argv[1]), b(argv[2]);
        const auto* central = &xoffset_scratch::detail::pool();
        need(a.api().registry_address() == central && b.api().registry_address() == central,
             "modules embedded independent registries");
        const auto payload = Limits::max_cached_block_bytes - ValidationScratch::overhead_for(16);
        need(a.api().cache_blocks(2, payload, 16), "module A active allocation failed");
        need(a.api().cached_bytes() == 2 * Limits::max_cached_block_bytes, "module A cache invisible");
        need(b.api().cache_blocks(3, payload, 16), "budget incorrectly rejected legal active input");
        need(a.api().cached_bytes() == Limits::max_idle_bytes &&
             b.api().cached_bytes() == Limits::max_idle_bytes &&
             ValidationScratch::statistics().cached_bytes == Limits::max_idle_bytes,
             "32 MiB idle budget was not shared across executable and two modules");
        a.api().clear_context();
        need(b.api().cached_bytes() == 2 * Limits::max_cached_block_bytes, "A trim lost B state");
        b.api().clear_context();
        need(ValidationScratch::statistics().retained_bytes == 0, "context survived module unload boundary");
    }
    need(ValidationScratch::statistics().retained_bytes == 0, "module unload retained scratch");
}
