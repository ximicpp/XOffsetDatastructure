// Capability probes, separate from the canonical model and performance cell.
#include "adapters/flatbuffers/adapter.hpp"
#include <iostream>

int main(){
    try{
        flatbuffers::FlatBufferBuilder builder;
        auto name=builder.CreateString("shared");
        auto record=sc_fb::CreateRecord(builder,1,2,3,name);
        std::vector<flatbuffers::Offset<sc_fb::Record>> references{record,record};
        auto rows=builder.CreateVector(references);
        auto snapshot=sc_fb::CreateSnapshot(builder,1,7,rows);sc_fb::FinishSnapshotBuffer(builder,snapshot);
        auto wire=builder.Release();
        auto require=[](bool b){if(!b)throw std::runtime_error("FlatBuffers capability mismatch");};
        flatbuffers::Verifier verifier(wire.data(),wire.size(),64,1000000);
        require(sc_fb::VerifySnapshotBuffer(verifier));
        auto root=sc_fb::GetSnapshot(wire.data());
        auto first=root->records()->Get(0),second=root->records()->Get(1);
        require(root->epoch()==7&&first==second&&first->score()==3);
        require(reinterpret_cast<const std::uint8_t*>(first)>=wire.data()&&reinterpret_cast<const std::uint8_t*>(first)<wire.data()+wire.size());
        require(const_cast<sc_fb::Record*>(first)->mutate_score(91));
        require(second->score()==91); // shared wire-table identity, not cycles
        flatbuffers::Verifier truncated(wire.data(),wire.size()/2,64,1000000);
        require(!sc_fb::VerifySnapshotBuffer(truncated));
        sc::Bytes malformed(wire.data(),wire.data()+wire.size());malformed[0]=malformed[1]=malformed[2]=malformed[3]=255;
        flatbuffers::Verifier corrupt(malformed.data(),malformed.size(),64,1000000);
        require(!sc_fb::VerifySnapshotBuffer(corrupt));
        std::cout<<"borrowed-read=passed wire-DAG-alias=passed scalar-inplace=passed verifier-selected-negative=passed native-cycles=unsupported U-mutable=not-established\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
