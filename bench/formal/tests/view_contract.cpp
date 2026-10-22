#include "views.hpp"
#include <iostream>

int main(){
    try{
        sc::Snapshot s;s.epoch=1;
        s.records={{1,3,-7,"abc",{0,4294967295u},{0,255},{{"k0001",{0,7,255}}}},
                   {5,0,42,"",{}, {}, {}}};
        std::vector<sc::Query> qs={{0,sc::QueryCode::record,1,""},{1,sc::QueryCode::record,5,""},
          {2,sc::QueryCode::record,4,""},{3,sc::QueryCode::attr,1,"k0001"},
          {4,sc::QueryCode::attr,1,"k9999"},{5,sc::QueryCode::attr,5,"k0001"},
          {6,sc::QueryCode::scan,0,""}};
        std::vector<sc::QueryResult> got(qs.size());
        sc::query_view(sc::PlainSnapshotView{s},qs,got);
        if(got!=sc::query_reference(s,qs))throw std::runtime_error("view/reference disagreement");
        if(got[0].values!=std::array<std::uint64_t,8>{3,std::uint64_t(-7),3,294,2,4294967295ULL,2,255})
            throw std::runtime_error("record golden mismatch");
        if(got[3].values!=std::array<std::uint64_t,8>{3,262,0,255,0,0,0,0})throw std::runtime_error("attribute golden mismatch");
        if(got[2].status!=sc::Status::not_found||got[4].status!=sc::Status::not_found)throw std::runtime_error("missing query status mismatch");
        if(sc::export_view(sc::PlainSnapshotView{s})!=s)throw std::runtime_error("view export mismatch");
        sc::Snapshot empty;sc::query_view(sc::PlainSnapshotView{empty},qs,got);
        if(got!=sc::query_reference(empty,qs))throw std::runtime_error("empty view mismatch");
        std::cout<<"view contract passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
