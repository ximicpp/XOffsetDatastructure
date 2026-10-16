"""F-only diagnostic transform. Reuses N5 event anchors; never changes production headers."""
from pathlib import Path
import hashlib, types, re
BASELINE = {
'xoffsetdatastructure.hpp':'9bc8f1f533bee83a9e111e05d67319383775dbca1844ef33b1aa717047640bb2',
'xoffset_virtual_memory.hpp':'22f181b52c11ce442a5576ffc0dc35af1828f77deb4424f387d0acfa658239ad'}

def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def generate(root):
    root=Path(root)
    for name,digest in BASELINE.items():
        if sha(root/name)!=digest: raise RuntimeError('not fixed F: '+name)
    path=root/'tools/run_vm_automatic_trace.py'
    if sha(path)!='339fce86f70d13d9df82ebb1ad5149339f1c866e124213cbc7adb62089a45b1e': raise RuntimeError('N5 source identity changed')
    mod=types.ModuleType('n3_n5');mod.__file__=str(path)
    exec(compile(path.read_text(),str(path),'exec'),mod.__dict__)
    core=mod.instrument((root/'xoffsetdatastructure.hpp').read_text())
    vm=(root/'xoffset_virtual_memory.hpp').read_text()
    anchors=[{'source':'tools/run_vm_automatic_trace.py','sha256':sha(path),'reuse':'instrument(F): all 13 N5 anchors validate once'}]
    def patch(text,old,new,n=1,label=None):
        count=text.count(old)
        if count!=n: raise RuntimeError(f'anchor drift ({label or old[:70]}): {count} != {n}')
        anchors.append({'anchor':label or old,'expected_count':n,'anchor_sha256':hashlib.sha256(old.encode()).hexdigest()})
        return text.replace(old,new)
    def c(old,new,n=1,label=None):
        nonlocal core
        core=patch(core,old,new,n,label)
    def v(old,new,n=1,label=None):
        nonlocal vm
        vm=patch(vm,old,new,n,label)
    event_map={
      'extent_request':'::n3::emit("extent_request", buffer_.data(), nullptr, required, buffer_.size());',
      'auto_growth':'::n3::emit("auto_growth", buffer_.data(), nullptr, buffer_.size(), current);',
      'core_reuse':'::n3::emit("core_reuse", buffer_.data(), buffer_.data()+block.user_offset, block.user_bytes, bytes);',
      'core_new':'::n3::emit("core_new", buffer_.data(), buffer_.data()+user_offset, bytes, block_end);',
      'expand_check':'::n3::emit("expand_check", buffer_.data(), ptr, bytes, header->user_bytes);',
      'expand_existing':'::n3::emit("expand_existing", buffer_.data(), ptr, bytes, header->user_bytes);',
      'expand_capacity_failure':'::n3::emit("expand_capacity_failure", buffer_.data(), ptr, bytes, header->user_bytes);',
      'expand_success':'::n3::emit("expand_success", buffer_.data(), ptr, bytes, header->user_bytes);',
      'core_return':'::n3::emit("core_return", buffer_.data(), ptr, header->total_bytes, free_blocks_.size());',
      'cache_flush':'::n3::emit("cache_flush", arena, nullptr, state->free_bytes, state->epoch);',
      'wire_reuse':'::n3::emit("wire_reuse", arena, block, block->block_bytes, aligned_bytes);',
      'wire_return':'::n3::emit("wire_return", arena, ptr, aligned_bytes, state->free_bytes);',
      'vector_relocation':'::n3::emit("vector_relocation_attempt", alloc.segment, fresh, std::uint64_t(size_)*sizeof(T), std::uint64_t(new_cap)*sizeof(T));'
    }
    for name,new in event_map.items():
        matches=re.findall(r'::vm_trace::emit\("'+name+r'", [^\n]*?\);',core)
        if len(matches)!=1: raise RuntimeError('N5 event adaptation drift: '+name)
        c(matches[0],new,label='N5 event '+name)
    c('#include "xoffset_virtual_memory.hpp"','#include "n3_runtime.hpp"\n#include "xoffset_virtual_memory.hpp"')
    c('std::vector<FreeBlock> free_blocks_;','std::vector<FreeBlock, ::n3::Allocator<FreeBlock,1>> free_blocks_;')
    c('std::unordered_map<const void*, XBufferCore*, AddressHash> owners;',
      'std::unordered_map<const void*, XBufferCore*, AddressHash, std::equal_to<const void*>, ::n3::Allocator<std::pair<const void* const, XBufferCore*>,2>> owners;')
    c('std::vector<WireValidationRange> owned_ranges;','std::vector<WireValidationRange, ::n3::Allocator<WireValidationRange,5>> owned_ranges;')
    c('std::vector<WireValidationRange> free_ranges;','std::vector<WireValidationRange, ::n3::Allocator<WireValidationRange,5>> free_ranges;')
    # Five registry locks share the same mutex; all lock scopes are preserved.
    n=core.count('std::lock_guard lock(state.mutex);')
    if n!=5: raise RuntimeError('registry lock inventory drift')
    c('std::lock_guard lock(state.mutex);','::n3::Lock lock(state.mutex, "registry_lock");',5)
    for sig in ['void* allocate_aligned_impl(std::uint32_t bytes, std::size_t alignment, bool allow_growth = true) {',
                'void deallocate_impl(void* ptr) noexcept {','void register_registry() {']:
        c(sig,sig+'\n            ::n3::Scope n3_scope(buffer_.data());')
    c('const auto found = state.owners.find(ptr);','const auto found = state.owners.find(ptr);\n            {::n3::Scope n3_lookup_scope(nullptr);::n3::emit(found != state.owners.end() ? "registry_lookup_hit" : "registry_lookup_miss", found != state.owners.end() ? found->second : nullptr, ptr);}')
    c('found->second = this;','found->second = this;\n            ::n3::emit("registry_rebind", this, buffer_.data());')
    c('if (buffer_.data()) state.owners.erase(buffer_.data());','if (buffer_.data()) {\n                const auto erased=state.owners.erase(buffer_.data());\n                if (erased) ::n3::emit("registry_erase", this, buffer_.data(), 0, erased);\n            }')
    c('if (std::exchange(detail::pending_registry_failure, false)) throw XBadAlloc();',
      'if (std::exchange(detail::pending_registry_failure, false)) {::n3::emit("registry_register_failed", this, buffer_.data());throw XBadAlloc();}')
    c('if (!result.second) throw XException("duplicate arena owner registration");',
      'if (!result.second) throw XException("duplicate arena owner registration");\n                ::n3::emit("registry_register", this, buffer_.data());\n                n3_metadata_snapshot();')
    c('// With nonthrowing hash/equality, failed insertion preserves all','::n3::emit("registry_register_failed", this, buffer_.data());\n                // With nonthrowing hash/equality, failed insertion preserves all')
    c('const auto& block = free_blocks_[i];','const auto& block = free_blocks_[i];\n                ::n3::emit("core_scan", buffer_.data(), buffer_.data()+block.user_offset);')
    c('free_blocks_.erase(free_blocks_.begin() + static_cast<std::ptrdiff_t>(i));',
      '::n3::emit("core_erase_movement_estimate", buffer_.data(), nullptr, (free_blocks_.size()-i-1)*sizeof(FreeBlock));\n                    free_blocks_.erase(free_blocks_.begin() + static_cast<std::ptrdiff_t>(i));\n                    n3_metadata_snapshot();')
    c('auto* block = freelist_block_from_offset(state, *link);','auto* block = freelist_block_from_offset(state, *link);\n                            ::n3::emit("wire_scan", arena, block);')
    c('arena->deallocate(block);','::n3::emit("wire_transfer_attempt", arena, block, block->block_bytes);\n                    arena->deallocate(block);')
    c('::n3::emit("core_return", buffer_.data(), ptr, header->total_bytes, free_blocks_.size());',
      '::n3::emit("core_return", buffer_.data(), ptr, header->total_bytes, free_blocks_.size());\n                n3_metadata_snapshot();')
    c('// Reclamation is best effort; destructors must not terminate on host OOM.',
      '::n3::emit("core_return_failed", buffer_.data(), ptr, header->total_bytes);\n                // Reclamation is best effort; destructors must not terminate on host OOM.')
    # Scoped snapshots are capacities, not allocation counts or whole-process totals.
    c('const void* root_type_identity_ = nullptr;', '''void n3_metadata_snapshot() const noexcept {
            ::n3::Scope n3_snapshot_scope(buffer_.data());
            ::n3::emit("core_metadata_capacity", buffer_.data(), this,
                free_blocks_.capacity()*sizeof(FreeBlock), sizeof(free_blocks_));
        }
        const void* root_type_identity_ = nullptr;''')
    c('other.update_registry_owner_locked(state);','other.update_registry_owner_locked(state);\n            n3_metadata_snapshot(); other.n3_metadata_snapshot();')
    c('            update_registry_owner_locked(state);\n        }','            update_registry_owner_locked(state);\n            n3_metadata_snapshot(); other.n3_metadata_snapshot();\n        }')
    # Distinguish failed tail-expansion decision groups without changing donor checks.
    begin=core.index('        bool try_expand_impl(');end=core.index('        void deallocate_impl(',begin)
    block=core[begin:end]
    if block.count('return false;')!=4: raise RuntimeError('expand rejection inventory drift')
    for reason in range(1,5):
        block=block.replace('return false;', '{::n3::emit("expand_rejected", buffer_.data(), ptr, bytes, '+str(reason)+');return N3_FALSE;}',1)
    block=block.replace('return N3_FALSE;','return false;')
    c(core[begin:end],block,label='four existing expansion failure groups')
    c('void reserve(std::size_t requested) {', 'void reserve(std::size_t requested) {\n            ::n3::Scope n3_container_scope(arena());',2)
    c('iterator insert_construct(size_type index, Construct&& construct) {','iterator insert_construct(size_type index, Construct&& construct) {\n            ::n3::Scope n3_container_scope(arena());')
    c('void erase_index(size_type index) {','void erase_index(size_type index) {\n            ::n3::Scope n3_container_scope(arena());')
    c('std::memcpy(dst, &src, sizeof(T));','::n3::copy("relocate_trivial_copy", nullptr, dst, &src, sizeof(T));')
    # Successful reserve relocations, rather than planned extent, are counted.
    c('fixed_detail::relocate_construct(fresh + i, old[i]);','fixed_detail::relocate_construct(fresh + i, old[i]);\n                    ::n3::emit("vector_relocated_element", alloc.segment, fresh+i, sizeof(T));')
    copies=[
      ('std::memcpy(new_data, old_data, size_)','::n3::copy("string_relocate_copy", alloc.segment, new_data, old_data, size_)'),
      ('std::memmove(data(), rhs.data(), rhs.size())','::n3::move("string_assign_copy", arena(), data(), rhs.data(), rhs.size())'),
      ('std::memmove(data() + index + 1, data() + index, (size_ - index) * sizeof(T))','::n3::move("vector_insert_shift", arena(), data() + index + 1, data() + index, (size_ - index) * sizeof(T))'),
      ('std::memcpy(data() + index, staged, sizeof(T))','::n3::copy("vector_insert_staged", arena(), data() + index, staged, sizeof(T))'),
      ('std::memmove(data() + index, data() + index + 1, (size_ - index - 1) * sizeof(T))','::n3::move("vector_erase_shift", arena(), data() + index, data() + index + 1, (size_ - index - 1) * sizeof(T))'),
      ('std::memcpy(output, &header, sizeof(header))','::n3::copy("save_header_copy", base, output, &header, sizeof(header))'),
      ('std::memcpy(output + sizeof(header), base, payload_size)','::n3::copy("save_payload_copy", base, output + sizeof(header), base, payload_size)'),
      ('std::memcpy(&header, data.data(), sizeof(header))','::n3::copy("load_header_copy", nullptr, &header, data.data(), sizeof(header))')]
    for old,new in copies:c(old,new)
    c('out.append(base, payload_size);','out.append(base, payload_size);\n            ::n3::emit("save_header_append_extent", base, nullptr, sizeof(header));\n            ::n3::emit("save_payload_append_extent", base, nullptr, payload_size);')
    c('load_verified(std::span<const std::byte> data,\n                                     std::optional<MaximumCapacity> cap = std::nullopt) {', 'load_verified(std::span<const std::byte> data,\n                                     std::optional<MaximumCapacity> cap = std::nullopt) {\n            ::n3::Scope n3_input_scope(nullptr);')
    c('static_cast<std::size_t>(header.reserved_bytes));', 'static_cast<std::size_t>(header.reserved_bytes));\n            ::n3::Scope n3_validation_scope(xbuf.data());')
    # copy_bytes string constructor and fallback appends are logical output extents, not libc internals.
    c('return buffer_.empty() ? std::string{} : std::string(buffer_.data(), buffer_.size());',
      'auto result=buffer_.empty() ? std::string{} : std::string(buffer_.data(), buffer_.size());\n            if (!buffer_.empty()) ::n3::emit("copy_bytes_string_extent", buffer_.data(), nullptr, result.size());\n            return result;')
    v('#include <cstddef>','#include <cstddef>\n#include "n3_runtime.hpp"')
    v('std::vector<VirtualMemoryStorage> cached;','std::vector<VirtualMemoryStorage, ::n3::Allocator<VirtualMemoryStorage,3>> cached;')
    v('std::make_shared<State>(limits)','std::allocate_shared<State>(::n3::Allocator<State,4>{}, limits)')
    v('std::lock_guard<std::mutex> lock(','::n3::Lock lock(',4)
    # Add domain argument to each of the four converted lock expressions.
    for expr in ['state_->mutex','state.mutex','state->mutex']:
        count=vm.count('::n3::Lock lock('+expr+');')
        if count:v('::n3::Lock lock('+expr+');','::n3::Lock lock('+expr+', "pool_lock");',count)
    v('if (state.cached[i].maximum_capacity_ != maximum_capacity_) continue;',
      '::n3::emit("pool_scan", nullptr, state.cached[i].base_, state.cached[i].maximum_capacity_);\n        if (state.cached[i].maximum_capacity_ != maximum_capacity_) continue;')
    for old,new,count in [('::mmap(','::n3::mmap_counted(',2),('::mprotect(','::n3::mprotect_counted(',2),('::munmap(','::n3::munmap_counted(',1),('::madvise(','::n3::madvise_counted(',1)]:v(old,new,count)
    v('std::memcpy(owner.base_, bytes, length);','::n3::copy("load_payload_copy", owner.base_, owner.base_, bytes, length);')
    v('std::memset(base_ + logical_bytes_, 0, new_logical_bytes - logical_bytes_);',
      '::n3::zero("grow_zero", base_, base_ + logical_bytes_, new_logical_bytes - logical_bytes_);')
    v('vm_pending_failures &= ~bit;', 'vm_pending_failures &= ~bit;\n    if (fail) ::n3::emit("vm_injected_failure", nullptr, nullptr, 0, bit);')
    return {'xoffsetdatastructure.hpp':core,'xoffset_virtual_memory.hpp':vm},anchors
