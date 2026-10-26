#include "xoffset_snapshot_index.hpp"
#include <iostream>
#include <stdexcept>
#if defined(XOFFSET_SNAPSHOT_INDEX_SHARED_RUNTIME)
#error This test must retain the header-only no-runtime link contract.
#endif
namespace xo = XOffsetDatastructure;
struct HeaderOnlyCell { xo::XString value; };
struct HeaderOnlyRoot { xo::XVector<HeaderOnlyCell> cells; };
XOFFSET_RECORD_DERIVED(HeaderOnlyCell, (), value)
XOFFSET_RECORD_DERIVED(HeaderOnlyRoot, (), cells)
XOFFSET_REGISTER_SCHEMA_NAME(HeaderOnlyRoot,"four-paths.snapshot-index-header-only.v1")
int main(){try{
    auto source=xo::TypedXBuffer<HeaderOnlyRoot>::create(65536);
    source.root().cells.resize(2);source.root().cells[0].value="a";source.root().cells[1].value="b";
    auto owner=xo::freeze_verified(std::move(source));
    auto index=xo::make_verified_snapshot_array_index(owner,owner.root().cells);
    if(index || xo::snapshot_index_detail::registry()!=nullptr ||
       !index.owns_address(owner.root().cells,&owner.root().cells[0].value))
        throw std::runtime_error("header-only mode did not preserve unknown/original-positive behavior");
    owner={};
    if(index.snapshot().root().cells[1].value!="b")throw std::runtime_error("disabled index lost its real immutable owner");
    std::cout<<"header-only disabled optimization/no optional runtime link passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
