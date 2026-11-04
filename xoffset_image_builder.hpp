#pragma once
// Experimental controlled producer for ordinary records, strings and vectors.
// Builds Wire 2.1 bytes directly. No owning T is placed in this storage.
#include "xoffset_image_view.hpp"

namespace XOffsetDatastructure::image {
namespace impl {
template<class T>struct build_vector : std::false_type {};
template<class E>struct build_vector<XFixedVector<E>> : std::true_type {};
template<class E>struct build_vector<XVector<E>> : std::true_type {};
template<class T> consteval bool build_supported();
template<class T,std::size_t I> consteval bool build_member() {
    constexpr auto m=std::meta::nonstatic_data_members_of(^^T,std::meta::access_context::unchecked())[I];
    if constexpr(std::meta::is_bit_field(m))return false;
    else {using M=[:std::meta::type_of(m):];return build_supported<M>();}
}
template<class T,std::size_t...I> consteval bool build_members(std::index_sequence<I...>) {
    return (build_member<T,I>() && ...);
}
template<class T> consteval bool build_supported() {
    if constexpr(std::is_const_v<T> || std::is_volatile_v<T> || wire_activation<T>::enabled)return false;
    else if constexpr(std::is_arithmetic_v<T> || std::is_enum_v<T>)return true;
    else if constexpr(detail::is_fixed_string_v<T>)return std::is_same_v<T,XFixedString> || std::is_same_v<T,XString>;
    // Sets/maps inherit vector storage but also require ordered keys. They must
    // not enter through the broad storage trait and bypass that invariant.
    else if constexpr(detail::is_fixed_flat_set_v<T> || detail::is_fixed_flat_map_v<T>)return false;
    else if constexpr(detail::is_fixed_vector_v<T>)return build_vector<T>::value && build_supported<typename T::value_type>();
    else if constexpr(std::is_array_v<T>)return build_supported<std::remove_extent_t<T>>();
    else if constexpr(std::is_class_v<T> && !std::is_union_v<T> && std::is_aggregate_v<T> && wire_record_layout<T>::ordinary_members) {
        constexpr auto bases=std::meta::bases_of(^^T,std::meta::access_context::unchecked()).size();
        if constexpr(bases!=0 || !detail::ordinary_record_members_disjoint<T>())return false;
        else return build_members<T>(std::make_index_sequence<detail::reflected_member_count<T>()>{});
    } else return false;
}
struct BuildStorage {
    std::shared_ptr<void> lifetime;
    std::string* wire{};
    std::size_t limit{};
    bool sealed=false,busy=false;
    void check()const {if(sealed || busy)throw XException("image builder: sealed or reentrant operation");}
    struct Guard {
        BuildStorage& s;
        explicit Guard(BuildStorage& s):s(s){s.check();s.busy=true;}
        ~Guard(){s.busy=false;}
    };
    // All fallible work precedes zeroing/linking. Call only under Guard.
    std::size_t append(std::size_t bytes) {
        if(bytes>limit || bytes>(std::numeric_limits<std::size_t>::max)()-7)
            throw XException("image builder: size overflow");
        bytes=(bytes+7)&~std::size_t(7);
        const auto old=wire->size();
        if(bytes>limit-old)throw XException("image builder: capacity limit");
        wire->reserve(old+bytes);
        wire->resize_and_overwrite(old+bytes,[&](char* p,std::size_t)noexcept{
            std::memset(p+old,0,bytes);return old+bytes;
        });
        return old;
    }
    RelSpan32 head(std::size_t at)const noexcept {
        return pod<RelSpan32>(reinterpret_cast<const std::byte*>(wire->data()+at));
    }
    std::size_t append_copy(const void* source,std::size_t bytes) {
        if(bytes>limit || bytes>(std::numeric_limits<std::size_t>::max)()-7)
            throw XException("image builder: size overflow");
        const auto rounded=(bytes+7)&~std::size_t(7);const auto old=wire->size();
        if(rounded>limit-old)throw XException("image builder: capacity limit");
        wire->reserve(old+rounded);
        wire->resize_and_overwrite(old+rounded,[&](char* p,std::size_t)noexcept{
            if(bytes)std::memcpy(p+old,source,bytes);
            std::memset(p+old+bytes,0,rounded-bytes);return old+rounded;
        });
        return old;
    }
    void head(std::size_t at,RelSpan32 value)noexcept{std::memcpy(wire->data()+at,&value,sizeof(value));}
};
}
template<class T>inline constexpr bool build_supported_v=impl::build_supported<T>();
template<class T>class BuildValue;
template<class T>class BuildSequence {
    std::shared_ptr<impl::BuildStorage> storage_;
    std::size_t at_,size_;
    BuildSequence(std::shared_ptr<impl::BuildStorage> s,std::size_t at,std::size_t n):storage_(std::move(s)),at_(at),size_(n){}
    template<class>friend class BuildValue;
public:
    std::size_t size()const noexcept{return size_;}
    BuildValue<T> operator[](std::size_t i)const {
        storage_->check();if(i>=size_)throw XException("image builder: index out of range");
        return BuildValue<T>(storage_,at_+i*sizeof(T));
    }
};
template<class T>class BuildValue {
    std::shared_ptr<impl::BuildStorage> storage_;
    std::size_t at_;
    BuildValue(std::shared_ptr<impl::BuildStorage> s,std::size_t at):storage_(std::move(s)),at_(at){}
    template<class>friend class ImageBuilder;
    template<class>friend class BuildValue;
    template<class>friend class BuildSequence;
public:
    template<std::size_t I>auto get()const
        requires(std::is_class_v<T> && !detail::is_fixed_string_v<T> && !detail::is_fixed_vector_v<T>) {
        storage_->check();
        constexpr auto field=std::meta::nonstatic_data_members_of(^^T,std::meta::access_context::unchecked())[I];
        using M=[:std::meta::type_of(field):];
        return BuildValue<M>(storage_,at_+std::meta::offset_of(field).bytes);
    }
    void set(T value) requires(std::is_arithmetic_v<T> || std::is_enum_v<T>) {
        impl::BuildStorage::Guard guard(*storage_);
        std::memcpy(storage_->wire->data()+at_,&value,sizeof(value));
    }
    auto resize(std::size_t n) requires(detail::is_fixed_vector_v<T>) {
        using E=typename T::value_type;
        impl::BuildStorage::Guard guard(*storage_);
        auto h=storage_->head(at_);
        if(h.capacity)throw XException("image builder: append-only vector already initialized");
        if(n>std::uint32_t(-1) || n>storage_->limit/sizeof(E))throw XException("image builder: size overflow");
        if(!n)return BuildSequence<E>(storage_,0,0);
        const auto block=storage_->append(n*sizeof(E));
        h={static_cast<std::int32_t>(block-at_),static_cast<std::uint32_t>(n),static_cast<std::uint32_t>(n)};
        storage_->head(at_,h);return BuildSequence<E>(storage_,block,n);
    }
    auto elements()const requires(std::is_array_v<T>) {
        storage_->check();return BuildSequence<std::remove_extent_t<T>>(storage_,at_,std::extent_v<T>);
    }
    template<class U=T>
    void assign(std::span<const typename U::value_type> values)
        requires(std::is_same_v<U,T> && detail::is_fixed_vector_v<U> && (std::is_arithmetic_v<typename U::value_type> || std::is_enum_v<typename U::value_type>)) {
        using E=typename U::value_type;impl::BuildStorage::Guard guard(*storage_);
        if(storage_->head(at_).capacity)throw XException("image builder: append-only vector already initialized");
        if(values.size()>std::uint32_t(-1) || values.size()>storage_->limit/sizeof(E))throw XException("image builder: size overflow");
        if(values.empty())return;
        const auto block=storage_->append_copy(values.data(),values.size_bytes());
        storage_->head(at_,{static_cast<std::int32_t>(block-at_),static_cast<std::uint32_t>(values.size()),static_cast<std::uint32_t>(values.size())});
    }
    void assign(std::string_view value) requires(detail::is_fixed_string_v<T>) {
        impl::BuildStorage::Guard guard(*storage_);
        auto h=storage_->head(at_);
        const bool heap=h.capacity && !(h.capacity&0x80000000u);
        if(value.size()>=0x80000000u || value.size()>storage_->limit)throw XException("image builder: size overflow");
        if(heap){
            if(value.size()>h.capacity)throw XException("image builder: append-only string capacity");
            auto* data=storage_->wire->data()+at_+h.delta;
            std::memcpy(data,value.data(),value.size());std::memset(data+value.size(),0,h.capacity+1-value.size());
            h.size=static_cast<std::uint32_t>(value.size());storage_->head(at_,h);return;
        }
        if(value.size()<=10){
            auto* out=storage_->wire->data()+at_;std::memset(out,0,12);
            if(value.size())std::memcpy(out,value.data(),value.size());
            if(value.size())out[11]=static_cast<char>(0x80u|value.size());return;
        }
        const auto block=storage_->append(value.size()+1);
        std::memcpy(storage_->wire->data()+block,value.data(),value.size());
        storage_->head(at_,{static_cast<std::int32_t>(block-at_),static_cast<std::uint32_t>(value.size()),static_cast<std::uint32_t>(value.size())});
    }
};

template<class T>class ImageBuilder {
    static_assert(build_supported_v<T> && is_v1_wire_admitted_v<T>,"image builder: unsupported controlled schema");
#ifdef XOFFSET_REJECT_IMPLICIT_PADDING
    static_assert(!has_implicit_padding_v<T>,"image builder: implicit padding rejected by configuration");
#endif
    std::shared_ptr<typename PackedWire<T>::State> packed_;
    std::shared_ptr<impl::BuildStorage> storage_;
public:
    explicit ImageBuilder(std::size_t maximum_wire_bytes):packed_(std::make_shared<typename PackedWire<T>::State>()),storage_(std::make_shared<impl::BuildStorage>()) {
        constexpr auto initial=sizeof(XWireHeaderV2)+((sizeof(T)+7)&~std::size_t(7));
        if(maximum_wire_bytes<initial || maximum_wire_bytes>XBufferCore::MAX_PAYLOAD-8)
            throw XException("image builder: invalid capacity limit");
        storage_->lifetime=packed_;storage_->wire=&packed_->wire;storage_->limit=maximum_wire_bytes;
        packed_->wire.reserve(maximum_wire_bytes);
        packed_->wire.resize(initial,0);packed_->root_offset=XWIRE_PAYLOAD_BEGIN;
    }
    ImageBuilder(const ImageBuilder&)=delete;
    ImageBuilder& operator=(const ImageBuilder&)=delete;
    ImageBuilder(ImageBuilder&&)=delete;
    ImageBuilder& operator=(ImageBuilder&&)=delete;
    BuildValue<T> root()const {storage_->check();return BuildValue<T>(storage_,sizeof(XWireHeaderV2));}
    std::size_t wire_size()const {storage_->check();return packed_->wire.size();}
    // O(1): all blocks were linked once, initialized and bounded at construction.
    // The resulting PackedWire is immutable even if old builder handles survive.
    PackedWire<T> seal() && {
        impl::BuildStorage::Guard guard(*storage_);
        XWireHeaderV2 header{};header.magic=XWIRE_MAGIC;header.flags=XWIRE_FLAGS;
        header.used_bytes=static_cast<std::uint32_t>(packed_->wire.size()+8);
        header.root_offset=XWIRE_PAYLOAD_BEGIN;
        header.schema_hash=detail::wire_schema_hash<T>();header.root_type_id=detail::wire_root_type_id<T>();
        std::memcpy(packed_->wire.data(),&header,sizeof(header));
        storage_->sealed=true;
        return PackedWire<T>(std::move(packed_));
    }
};
}
