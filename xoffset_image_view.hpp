#pragma once
// Experimental read-only byte proxy. Never creates an owning T in copied bytes.
#include "xoffset_dense_export.hpp"
#include <string_view>

namespace XOffsetDatastructure::image {
// A passive wire descriptor, never used to alias an owning head in storage.
struct RelSpan32 { std::int32_t delta; std::uint32_t size, capacity; };
static_assert(sizeof(RelSpan32) == 12 && std::is_trivially_copyable_v<RelSpan32>);

namespace impl {
template<class T> consteval bool supported();
template<class T, std::size_t I> consteval bool member_supported() {
    constexpr auto field = std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked())[I];
    if constexpr (std::meta::is_bit_field(field)) return false;
    else { using M = [:std::meta::type_of(field):]; return supported<M>(); }
}
template<class T, std::size_t... I> consteval bool members(std::index_sequence<I...>) {
    return (member_supported<T, I>() && ...);
}
template<class T> consteval bool supported() {
    if constexpr (std::is_const_v<T> || std::is_volatile_v<T>) return false;
    else if constexpr (std::is_arithmetic_v<T> || std::is_enum_v<T>) return true;
    else if constexpr (detail::is_fixed_string_v<T>) return true;
    else if constexpr (detail::is_fixed_vector_v<T>) return supported<typename T::value_type>();
    else if constexpr (std::is_array_v<T>) return supported<std::remove_extent_t<T>>();
    else if constexpr (std::is_class_v<T> && wire_record_layout<T>::ordinary_members) {
        constexpr auto bases = std::meta::bases_of(^^T, std::meta::access_context::unchecked()).size();
        if constexpr (bases != 0) return false;
        else return members<T>(std::make_index_sequence<detail::reflected_member_count<T>()>{});
    } else return false;
}
template<class T> T pod(const std::byte* address) noexcept {
    static_assert(std::is_trivially_copyable_v<T>);
    T result;
    std::memcpy(&result, address, sizeof(T));
    return result;
}
template<class T> auto read(const std::byte* address);
}

// Child proxies and string_views borrow the enclosing ImageView, like a span.
template<class T> class RecordView {
    const std::byte* address_;
    explicit RecordView(const std::byte* address) noexcept : address_(address) {}
    template<class U> friend auto impl::read(const std::byte*);
public:
    template<std::size_t I> auto get() const {
        constexpr auto field = std::meta::nonstatic_data_members_of(^^T,
            std::meta::access_context::unchecked())[I];
        using M = [:std::meta::type_of(field):];
        return impl::read<M>(address_ + std::meta::offset_of(field).bytes);
    }
};
template<class E> class SequenceView {
    const std::byte* data_;
    std::size_t size_, capacity_;
    SequenceView(const std::byte* data, std::size_t size, std::size_t capacity) noexcept
        : data_(data), size_(size), capacity_(capacity) {}
    template<class U> friend auto impl::read(const std::byte*);
public:
    std::size_t size() const noexcept { return size_; }
    std::size_t capacity() const noexcept { return capacity_; }
    auto operator[](std::size_t index) const {
        if (index >= size_) throw XException("image view: index out of range");
        return impl::read<E>(data_ + index * sizeof(E));
    }
};
namespace impl {
template<class T> auto read(const std::byte* address) {
    if constexpr (std::is_arithmetic_v<T> || std::is_enum_v<T>) return pod<T>(address);
    else if constexpr (detail::is_fixed_string_v<T>) {
        const auto h = pod<RelSpan32>(address);
        if (h.capacity & 0x80000000u)
            return std::string_view(reinterpret_cast<const char*>(address), (h.capacity >> 24) & 0x7fu);
        return std::string_view(h.size ? reinterpret_cast<const char*>(address + h.delta) : "", h.size);
    } else if constexpr (detail::is_fixed_vector_v<T>) {
        const auto h = pod<RelSpan32>(address);
        return SequenceView<typename T::value_type>(h.capacity ? address + h.delta : nullptr, h.size, h.capacity);
    } else if constexpr (std::is_array_v<T>) {
        return SequenceView<std::remove_extent_t<T>>(address, std::extent_v<T>, std::extent_v<T>);
    } else return RecordView<T>(address);
}
struct Storage {
    std::unique_ptr<std::byte[]> data;
    std::size_t capacity;
    explicit Storage(std::size_t n) : data(new std::byte[n]), capacity(n) {
        // Explicit destination preparation, including first touch, outside copy.
        std::memset(data.get(), 0, n);
    }
};
}
template<class T> inline constexpr bool supported_v = impl::supported<T>();
template<class T> class PreparedImageSlot;
template<class T> class ImageView;
template<class T> ImageView<T> copy_image_into(const PackedWire<T>&, PreparedImageSlot<T>&);

template<class T> class ImageView {
    std::shared_ptr<const impl::Storage> owner_;
    std::uint32_t root_offset_{};
    std::size_t used_{};
    ImageView(std::shared_ptr<const impl::Storage> owner, std::uint32_t root, std::size_t used) noexcept
        : owner_(std::move(owner)), root_offset_(root), used_(used) {}
    friend ImageView<T> copy_image_into<T>(const PackedWire<T>&, PreparedImageSlot<T>&);
public:
    ImageView() noexcept = default;
    explicit operator bool() const noexcept { return bool(owner_); }
    auto root() const {
        if (!owner_) throw XException("image view: empty view");
        // Wire 2.1 removes the 48-byte arena header and adds a 40-byte wire header.
        return impl::read<T>(owner_->data.get() + root_offset_ - XWIRE_PAYLOAD_BEGIN + sizeof(XWireHeaderV2));
    }
    std::span<const std::byte> bytes() const noexcept {
        return owner_ ? std::span<const std::byte>(owner_->data.get(), used_) : std::span<const std::byte>{};
    }
};

template<class T> class PreparedImageSlot {
    static_assert(supported_v<T>, "image view: unsupported proxy schema");
    std::shared_ptr<impl::Storage> owner_;
    friend ImageView<T> copy_image_into<T>(const PackedWire<T>&, PreparedImageSlot<T>&);
public:
    explicit PreparedImageSlot(std::size_t capacity) : owner_(std::make_shared<impl::Storage>(capacity)) {}
    PreparedImageSlot(PreparedImageSlot&&) noexcept = default;
    PreparedImageSlot& operator=(PreparedImageSlot&&) noexcept = default;
    PreparedImageSlot(const PreparedImageSlot&) = delete;
    PreparedImageSlot& operator=(const PreparedImageSlot&) = delete;
};

// Typed immutable provenance comes from freeze_verified + pack_dense. No byte
// input / trusted flag. No owning container lifetime is started or assumed.
// Slot use is single-threaded; a retained view prevents overwrite. Source must
// obey the snapshot's no-writable-alias contract. Preflight rejection writes none.
template<class T>
ImageView<T> copy_image_into(const PackedWire<T>& source, PreparedImageSlot<T>& destination) {
    static_assert(supported_v<T>);
    if (!destination.owner_ || destination.owner_.use_count() != 1)
        throw XException("image copy: empty slot or outstanding view");
    if (!source) throw XException("image copy: empty source");
    const auto bytes = source.bytes();
    if (bytes.size() > destination.owner_->capacity)
        throw XException("image copy: insufficient destination capacity");
    const auto root = source.root_offset();
    std::memmove(destination.owner_->data.get(), bytes.data(), bytes.size());
    return ImageView<T>(destination.owner_, root, bytes.size());
}

// Explicit borrowed alternative. As with XBuffer::root()/span, all child
// proxies and strings are invalidated by reload, move or buffer destruction.
// The caller excludes concurrent reads/writes and releases old borrows before
// reload. No shared ownership or runtime enforcement of outstanding borrows.
template<class T> class BorrowedImageBuffer {
    static_assert(supported_v<T>, "image buffer: unsupported proxy schema");
    impl::Storage storage_;
    std::uint32_t root_offset_{};
    std::size_t used_{};
    std::uint64_t epoch_{};
public:
    explicit BorrowedImageBuffer(std::size_t capacity) : storage_(capacity) {}
    BorrowedImageBuffer(const BorrowedImageBuffer&) = delete;
    BorrowedImageBuffer& operator=(const BorrowedImageBuffer&) = delete;
    BorrowedImageBuffer(BorrowedImageBuffer&& other) noexcept
        : storage_(std::move(other.storage_)), root_offset_(other.root_offset_),
          used_(std::exchange(other.used_,0)), epoch_(other.epoch_) { ++other.epoch_; }
    BorrowedImageBuffer& operator=(BorrowedImageBuffer&& other) noexcept {
        if (this != &other) {
            storage_=std::move(other.storage_);root_offset_=other.root_offset_;
            used_=std::exchange(other.used_,0);++epoch_;++other.epoch_;
        }
        return *this;
    }
    void reload(const PackedWire<T>& source) {
        if (!source) throw XException("image buffer: empty source");
        const auto bytes=source.bytes();
        if (!storage_.data || bytes.size()>storage_.capacity)
            throw XException("image buffer: insufficient destination capacity");
        const auto root=source.root_offset();
        std::memmove(storage_.data.get(),bytes.data(),bytes.size());
        root_offset_=root;used_=bytes.size();++epoch_;
    }
    auto root() const & {
        if (!used_) throw XException("image buffer: empty buffer");
        return impl::read<T>(storage_.data.get()+root_offset_-XWIRE_PAYLOAD_BEGIN+sizeof(XWireHeaderV2));
    }
    auto root() const && = delete;
    std::span<const std::byte> bytes() const & noexcept {
        return {storage_.data.get(),used_};
    }
    std::span<const std::byte> bytes() const && = delete;
    std::uint64_t epoch() const noexcept { return epoch_; }
};
}
