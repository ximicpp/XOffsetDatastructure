@0xe8c4a12937f54d11;

$import "/capnp/c++.capnp".namespace("capnp_rich_test");

struct Position {
  x @0 :Float32;
  y @1 :Float32;
  z @2 :Float32;
}

struct Affix {
  attrId @0 :UInt32;
  value @1 :Float32;
}

struct Gem {
  id @0 :UInt32;
  tier @1 :UInt32;
  name @2 :Text;
  bonuses @3 :List(Affix);
}

struct RichItem {
  id @0 :UInt32;
  uuid @1 :UInt64;
  number @2 :UInt32;
  timestamp @3 :UInt64;
  name @4 :Text;
  affixes @5 :List(Affix);
  flags @6 :List(UInt32);
}

struct RichItemEntry {
  key @0 :UInt64;
  value @1 :RichItem;
}

struct RichEquip {
  id @0 :UInt32;
  uuid @1 :UInt64;
  number @2 :UInt32;
  timestamp @3 :UInt64;
  level @4 :UInt32;
  name @5 :Text;
  creator @6 :Text;
  attributes @7 :List(Float32);
  gems @8 :List(Gem);
}

struct SkillEffect {
  effectId @0 :UInt32;
  magnitude @1 :Float32;
  duration @2 :Float32;
  tag @3 :Text;
}

struct RichSkill {
  id @0 :UInt32;
  level @1 :UInt32;
  cooldownMs @2 :UInt32;
  name @3 :Text;
  effects @4 :List(SkillEffect);
}

struct QuestObjective {
  targetId @0 :UInt32;
  current @1 :UInt32;
  required @2 :UInt32;
  desc @3 :Text;
}

struct Quest {
  questId @0 :UInt32;
  status @1 :UInt32;
  acceptedAt @2 :UInt64;
  title @3 :Text;
  objectives @4 :List(QuestObjective);
}

struct QuestEntry {
  key @0 :UInt32;
  value @1 :Quest;
}

struct RichCharacter {
  id @0 :UInt64;
  name @1 :Text;
  guildName @2 :Text;
  title @3 :Text;
  level @4 :UInt32;
  healthpoint @5 :Float32;
  manapoint @6 :Float32;
  speed @7 :Float32;
  pos @8 :Position;
  items @9 :List(RichItemEntry);
  path @10 :List(Position);
  attributes @11 :List(Float32);
  equips @12 :List(RichEquip);
  skills @13 :List(RichSkill);
  quests @14 :List(QuestEntry);
}
