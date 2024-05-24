@0xbf5147cbbecf40c1;

$import "/capnp/c++.capnp".namespace("capnp_character_test");

struct Skill {
  id @0 :UInt32;
  level @1 :UInt32;
}

struct Item {
  id @0 :UInt32;
  uuid @1 :UInt64;
  number @2 :UInt32;
  timestamp @3 :UInt64;
}

struct Equip {
  id @0 :UInt32;
  uuid @1 :UInt64;
  number @2 :UInt32;
  timestamp @3 :UInt64;
  level @4 :UInt32;
  attributes @5 :List(Float32);
}

struct Position {
  x @0 :Float32;
  y @1 :Float32;
  z @2 :Float32;
}

struct Map {
  entries @0 :List(Entry);
  struct Entry {
    key @0 :UInt64;
    value @1 :Item;
  }
}

struct Character {
  id @0 :UInt64;
  name @1 :Text;
  level @2 :UInt32;
  healthpoint @3 :Float32;
  manapoint @4 :Float32;
  speed @5 :Float32;
  pos @6 :Position;
  path @7 :List(Position);
  attributes @8 :List(Float32);
  items @9 :Map;
  equips @10 :List(Equip);
  skills @11 :List(Skill);
}