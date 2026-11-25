// Map state demo: the schema freeze tool for the demo's root types
// (xoffset_schema_freeze.hpp, docs/SCHEMA_EVOLUTION.md section 7). Built with
// the servers (C++26 reflection); OWNER: integration thread.
//
// At a release, from demo/mapstate/include (the snapshot's #include lines
// start there):
//   mapdemo_schema_freeze freeze v2 mapdemo_v2 mapdemo/schema_v2.hpp ../schema.lock
// On every run (tools/run_all_tiers.sh):
//   mapdemo_schema_freeze check demo/mapstate/schema.lock
// fails when a root type changed its fingerprint and is neither a later
// version of the last frozen one (XOFFSET_SCHEMA_FAMILY: fields added in its
// XSpare bytes) nor a declared new generation, or kept the fingerprint under
// other field names.
#include "mapdemo/schema.hpp"
#include "xoffset_schema_freeze.hpp"

int main(int argc, char** argv) {
    using namespace mapdemo;
    return XOffsetDatastructure::schema_freeze_main<MapCatalog, UnitStateBatch, ViewRequest, ViewSnapshot, UnitEvent>(
        argc, argv);
}
