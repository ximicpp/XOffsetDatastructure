// Map state demo: the schema freeze tool for the demo's root types
// (xoffset_schema_freeze.hpp, docs/SCHEMA_EVOLUTION.md section 5). Built with
// the servers (C++26 reflection); OWNER: integration thread.
//
// At a release, from demo/mapstate/include (the snapshot's #include lines
// start there):
//   mapdemo_schema_freeze freeze v2 mapdemo_v2 mapdemo/schema_v2.hpp ../schema.lock
// then list the snapshot types in schema_lineage.hpp. On every run
// (tools/run_all_tiers.sh):
//   mapdemo_schema_freeze check demo/mapstate/schema.lock
// fails when a root type changed its layout without a lineage entry for the
// last frozen version, or kept the layout under other field names.
#include "mapdemo/schema_lineage.hpp"
#include "xoffset_schema_freeze.hpp"

int main(int argc, char** argv) {
    using namespace mapdemo;
    return XOffsetDatastructure::schema_freeze_main<MapCatalog, UnitStateBatch, ViewRequest, ViewSnapshot, UnitEvent>(
        argc, argv);
}
