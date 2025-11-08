#ifndef PLAYER_HPP_
#define PLAYER_HPP_

#include "../xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// struct alignas(XTypeSignature::BASIC_ALIGNMENT) Player {
class alignas(8) Player {
public:
	// Default constructor
	template <typename Allocator>
	Player(Allocator allocator) : name(allocator), items(allocator) {}

	// Full constructor for emplace_back
	template <typename Allocator>
	Player(Allocator allocator, int id_val, int level_val, const char* name_val)
		: id(id_val)
		, level(level_val)
		, name(name_val, allocator)
		, items(allocator)
	{}

	int32_t id{0};
	int32_t level{0};
	XString name;
	XVector<int32_t> items;
};

// Full type signature validation using C++26 reflection
static_assert(XTypeSignature::get_XTypeSignature<Player>() ==
             "struct[s:72,a:8]{"
             "@0:i32[s:4,a:4],"
             "@4:i32[s:4,a:4],"
             "@8:string[s:32,a:8],"
             "@40:vector[s:32,a:8]<i32[s:4,a:4]>}",
              "Type signature mismatch for Player - "
              "Binary layout changed! This breaks serialization compatibility.");

#endif // PLAYER_HPP_
