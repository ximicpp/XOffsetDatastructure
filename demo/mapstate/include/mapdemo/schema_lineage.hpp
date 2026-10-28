// Map state demo: the versions of each root type that A still serves
// (xoffset_evolution.hpp, docs/SCHEMA_EVOLUTION.md). Server side only: it
// needs C++26 reflection. OWNER: servers thread.
//
// Version 1 (schema_v1.hpp, frozen 10-28 by src/schema_freeze.cpp) is the
// schema of the release before ViewSnapshot gained scene_units. The other
// root types did not change, so they have no lineage: a version-1 peer works
// in the same MapCatalog, UnitStateBatch, ViewRequest and UnitEvent.
//
// A version-1 client gets its snapshots converted: A builds the working
// ViewSnapshot, XPeerSchemas::save_into() converts it (scene_units and
// reserved0 are dropped, every other field is matched by name) and writes the
// version-1 wire with save_compact.
#pragma once
#include "mapdemo/schema.hpp"
#include "mapdemo/schema_v1.hpp"
#include "xoffset_evolution.hpp"

XOFFSET_SCHEMA_LINEAGE(mapdemo::ViewSnapshot, mapdemo_v1::ViewSnapshot)
