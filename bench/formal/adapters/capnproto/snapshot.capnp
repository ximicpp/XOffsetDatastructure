@0xbfd8b11b73b84167;
using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("sc_cap");

struct Attr {
  key @0 :Text;
  value @1 :Data;
}
struct Record {
  id @0 :UInt64;
  revision @1 :UInt32;
  score @2 :Int64;
  name @3 :Text;
  samples @4 :List(UInt32);
  payload @5 :Data;
  attrs @6 :List(Attr);
}
struct Snapshot {
  schemaVersion @0 :UInt32;
  epoch @1 :UInt64;
  records @2 :List(Record);
}
