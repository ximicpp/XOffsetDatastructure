#ifndef PLAYER_HPP_
#define PLAYER_HPP_

#include "../xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

class alignas(8) Player {
public:
	using allocator_type = XAllocator;

	// Allocator constructor (suffix mode — standard uses_allocator protocol).
	// The allocator_arg_t constraint prevents scoped_allocator_adaptor's
	// prefix-mode detection from matching this single-arg template.
	template <typename Allocator>
		requires (!std::is_same_v<std::decay_t<Allocator>, std::allocator_arg_t>)
	Player(Allocator allocator) : name(allocator), items(allocator) {}

	// Move + allocator constructor (required for vector reallocation
	// when scoped_allocator_adaptor moves elements).
	template <typename Allocator>
	Player(Player&& other, Allocator allocator)
		: id(other.id)
		, level(other.level)
		, name(std::move(other.name), allocator)
		, items(std::move(other.items), allocator)
	{}

	int32_t id{0};
	int32_t level{0};
	XString name;
	XVector<int32_t> items;
};

// Full type signature validation using boost::typelayout (Definition Signature)
static_assert(boost::typelayout::get_definition_signature<Player>() ==
             "[64-le]record[s:72,a:8]{"
             "@0[id]:i32[s:4,a:4],"
             "@4[level]:i32[s:4,a:4],"
             "@8[name]:string[s:32,a:8],"
             "@40[items]:vector[s:32,a:8]<i32[s:4,a:4]>}",
              "Type signature mismatch for Player - "
              "Binary layout changed! This breaks serialization compatibility.");

#endif // PLAYER_HPP_
