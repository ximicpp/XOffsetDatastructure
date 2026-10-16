// Experimental synchronous POSIX TCP loopback consumer, not a public I/O API.
#include <xoffsetdatastructure.hpp>
#include "network_support.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#ifndef N7_NET_DIAGNOSTICS
#define N7_NET_DIAGNOSTICS 1
#endif
#if N7_NET_DIAGNOSTICS != 0 && N7_NET_DIAGNOSTICS != 1
#error "N7_NET_DIAGNOSTICS must be 0 or 1"
#endif
using namespace XOffsetDatastructure;
namespace fs = std::filesystem;
struct IoReferenceRoot { std::uint32_t marker; XVector<std::uint32_t> values; XString label; };
XOFFSET_REGISTER_SCHEMA_NAME(IoReferenceRoot, "n7.reference.file-pipeline.v1")
#include "frame_model.hpp"
#include "frame_model_tests.hpp"
namespace {
using Clock=std::chrono::steady_clock;
using Buffer=TypedXBuffer<IoReferenceRoot>;
constexpr bool diagnostic=N7_NET_DIAGNOSTICS==1;
constexpr std::size_t H=sizeof(XWireHeaderV1);
static_assert(H==88);
constexpr std::uint32_t input_marker=0x4e37494fu, output_marker=input_marker^0xa55aa55au;
constexpr std::string_view input_label="n7.reference.v1.input", output_label="n7.reference.v1.ready";
constexpr std::size_t chunk_bytes=65536;
struct Failure:std::runtime_error { std::string reason; Failure(std::string r,std::string message):std::runtime_error(std::move(message)),reason(std::move(r)){} };
void require(bool ok,std::string_view message) { if(!ok) throw Failure("precondition",std::string(message)); }
std::uint64_t elapsed(Clock::time_point t) { return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-t).count(); }
std::string quoted(std::string_view s) { std::string r="\""; const char* hex="0123456789abcdef"; for(unsigned char c:s) { if(c=='\\'||c=='"'){r+='\\';r+=c;}else if(c<32){r+="\\u00";r+=hex[c>>4];r+=hex[c&15];}else r+=c;}return r+'"'; }
std::uint32_t fixture_value(std::size_t index) {
    const auto i = static_cast<std::uint32_t>(index);
    return (i * 2654435761u) ^ (i >> 7) ^ 0x39ac17e5u;
}

std::uint32_t processed_value(std::uint32_t value) {
    return ((value << 7) | (value >> 25)) + 0x1020304u;
}

// Field values, count and label bytes are hashed in a defined little-endian
// order. This is a convenience identifier, never the semantic equality test.
std::uint64_t semantic_checksum(const IoReferenceRoot& root) {
    std::uint64_t result = 14695981039346656037ull;
    const auto byte = [&](unsigned char value) { result = (result ^ value) * 1099511628211ull; };
    const auto integer = [&](std::uint64_t value) {
        for (unsigned shift = 0; shift != 64; shift += 8) byte((value >> shift) & 255u);
    };
    integer(root.marker); integer(root.values.size());
    for (const auto value : root.values) integer(value);
    integer(root.label.size());
    for (const unsigned char value : root.label.view()) byte(value);
    return result;
}

void verify_values(const IoReferenceRoot& root, bool processed, std::size_t count) {
    require(root.marker == (processed ? output_marker : input_marker), "unexpected marker");
    require(root.label.view() == (processed ? output_label : input_label), "unexpected label");
    require(root.values.size() == count, "unexpected element count");
    for (std::size_t i = 0; i < count; ++i) {
        const auto expected = processed ? processed_value(fixture_value(i)) : fixture_value(i);
        require(root.values[i] == expected, "semantic value mismatch at " + std::to_string(i));
    }
}

void process(IoReferenceRoot& root, std::size_t count) {
    require(root.marker == input_marker && root.label.view() == input_label,
            "input changed since preflight");
    require(root.values.size() == count, "input count changed since preflight");
    for (std::size_t i = 0; i < count; ++i) {
        require(root.values[i] == fixture_value(i), "unexpected input value");
        root.values[i] = processed_value(root.values[i]);
    }
    root.marker = output_marker;
    root.label = output_label;
}

struct IOProgress { std::size_t read=0,written=0,allocations=0,waits=0; bool published=false; std::size_t terminal_count=0; std::uint64_t n3_rx_size=0,n3_rx_capacity=0,n3_rx_init_extent=0,n3_header_copy_calls=0,n3_header_copy_bytes=0; };
struct TerminalEvent {
    std::size_t* count;explicit TerminalEvent(std::size_t* p):count(p){}
    TerminalEvent(const TerminalEvent&)=delete;
    ~TerminalEvent(){if(count)++*count;}
};
struct Socket {
    inline static std::size_t live_count=0;
    int fd=-1;
    Socket()=default; explicit Socket(int n):fd(n){require(n>=0,"socket creation failed");++live_count;}
    Socket(const Socket&)=delete; Socket& operator=(const Socket&)=delete;
    Socket(Socket&& x) noexcept:fd(std::exchange(x.fd,-1)){}
    Socket& operator=(Socket&& x) noexcept {if(this!=&x){close();fd=std::exchange(x.fd,-1);}return *this;}
    void close() noexcept { if(fd>=0){::close(fd);fd=-1;--live_count;} }
    ~Socket(){close();}
};
void nonblocking(int fd) {const int flags=fcntl(fd,F_GETFL,0);require(flags>=0&&fcntl(fd,F_SETFL,flags|O_NONBLOCK)==0,"nonblocking configuration failed");
#ifdef SO_NOSIGPIPE
    int one=1;require(setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&one,sizeof(one))==0,"SO_NOSIGPIPE failed");
#endif
}
struct Deadline {
    Clock::time_point end;
    explicit Deadline(std::size_t ms):end(Clock::now()+std::chrono::milliseconds(ms)){}
    void check() const {if(Clock::now()>=end)throw Failure("timeout","operation deadline expired");}
    void wait(int fd,short events,IOProgress* p=nullptr) const {
        for(;;){const auto now=Clock::now();if(now>=end)throw Failure("timeout","operation deadline expired");
            const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(end-now).count();
            const int ms=static_cast<int>(std::min<std::int64_t>(600000,(ns+999999)/1000000));
            pollfd descriptor{fd,events,0};if(p)++p->waits;const int n=poll(&descriptor,1,ms);
            if(n>0)return;if(n==0)throw Failure("timeout","operation deadline expired");
            if(errno!=EINTR)throw Failure("poll_error",std::strerror(errno));}
    }
};
struct Connection {Socket client,server;};
Connection connect_loopback(std::size_t timeout) {
    Deadline deadline(timeout);Socket listener(::socket(AF_INET,SOCK_STREAM,0));nonblocking(listener.fd);
    sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    require(bind(listener.fd,reinterpret_cast<sockaddr*>(&address),sizeof(address))==0,"loopback bind failed");
    socklen_t length=sizeof(address);require(getsockname(listener.fd,reinterpret_cast<sockaddr*>(&address),&length)==0,"getsockname failed");
    require(listen(listener.fd,1)==0,"listen failed");Socket client(::socket(AF_INET,SOCK_STREAM,0));nonblocking(client.fd);
    if(connect(client.fd,reinterpret_cast<sockaddr*>(&address),sizeof(address))!=0){require(errno==EINPROGRESS,"connect failed");deadline.wait(client.fd,POLLOUT);int error=0;length=sizeof(error);require(getsockopt(client.fd,SOL_SOCKET,SO_ERROR,&error,&length)==0&&error==0,"connect completion failed");}
    int accepted=-1;while(accepted<0){deadline.check();accepted=accept(listener.fd,nullptr,nullptr);if(accepted<0){require(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR,"accept failed");deadline.wait(listener.fd,POLLIN);}}
    Socket server(accepted);nonblocking(server.fd);int one=1;
    require(setsockopt(client.fd,IPPROTO_TCP,TCP_NODELAY,&one,sizeof(one))==0&&setsockopt(server.fd,IPPROTO_TCP,TCP_NODELAY,&one,sizeof(one))==0,"TCP_NODELAY failed");
    deadline.check();return {std::move(client),std::move(server)};
}
void read_exact(int fd,char* output,std::size_t size,const Deadline& deadline,IOProgress& p,std::string_view eof_reason,std::size_t chunk=chunk_bytes) {
    std::size_t done=0;while(done<size){deadline.check();const auto n=recv(fd,output+done,std::min(size-done,chunk),0);
        if(n>0){done+=n;p.read+=n;continue;}
        if(n==0)throw Failure(std::string(eof_reason),"peer closed before complete frame");
        if(errno==EINTR)continue;if(errno==EAGAIN||errno==EWOULDBLOCK){deadline.wait(fd,POLLIN,&p);continue;}
        throw Failure("read_error",std::strerror(errno));}deadline.check();
}
void send_exact(int fd,std::string_view wire,const Deadline& deadline,IOProgress& p,std::size_t chunk=chunk_bytes) {
    std::size_t done=0;while(done<wire.size()){deadline.check();
#ifdef MSG_NOSIGNAL
        constexpr int flags=MSG_NOSIGNAL;
#else
        constexpr int flags=0;
#endif
        const auto n=send(fd,wire.data()+done,std::min(wire.size()-done,chunk),flags);
        if(n>0){done+=n;p.written+=n;continue;}if(n==0)throw Failure("zero_write","zero-progress send");
        if(errno==EINTR)continue;if(errno==EAGAIN||errno==EWOULDBLOCK){deadline.wait(fd,POLLOUT,&p);continue;}
        throw Failure("write_error",std::strerror(errno));}deadline.check();
}
constexpr auto type_id=wire_root_type_id_v<IoReferenceRoot>();
constexpr auto schema_hash=wire_schema_hash_v<IoReferenceRoot>();
std::string receive_frame(int fd,const vm_network::Limits& limits,const Deadline& deadline,IOProgress& progress,std::size_t chunk=chunk_bytes) {
    TerminalEvent completion(&progress.terminal_count);
    XWireHeaderV1 header{};read_exact(fd,reinterpret_cast<char*>(&header),H,deadline,progress,"eof_header",chunk);
    const auto admission=vm_network::admit_header(header,limits,type_id,schema_hash);
    if(!admission)throw Failure(std::string(vm_network::error_name(admission.error)),"frame header or budget rejected before allocation");
    std::string wire(admission.wire_bytes,'\0');++progress.allocations;std::memcpy(wire.data(),&header,H);progress.n3_rx_size=wire.size();progress.n3_rx_capacity=wire.capacity();progress.n3_rx_init_extent=admission.wire_bytes;++progress.n3_header_copy_calls;progress.n3_header_copy_bytes+=H;
    read_exact(fd,wire.data()+H,wire.size()-H,deadline,progress,"eof_payload",chunk);return wire;
}
void write_exclusive(const fs::path& file,std::string_view bytes,bool* created=nullptr) {
    std::FILE* f=std::fopen(file.string().c_str(),"wbx");require(f!=nullptr,"output already exists or cannot be created");
    if(created)*created=true;
    bool ok=std::fwrite(bytes.data(),1,bytes.size(),f)==bytes.size();const int closed=std::fclose(f);require(ok&&closed==0,"output write/close failed");
}
std::string read_file(const fs::path& file,std::size_t maximum) {
    require(fs::is_regular_file(file),"input must be a regular file");const auto size=fs::file_size(file);require(size>=H&&size<=maximum,"input file outside max-wire");
    std::string wire(size,'\0');std::FILE* f=std::fopen(file.string().c_str(),"rb");require(f!=nullptr,"cannot open input");
    const auto received=std::fread(wire.data(),1,wire.size(),f);const int extra=std::fgetc(f);const int error=std::ferror(f);const int closed=std::fclose(f);
    require(received==wire.size()&&extra==EOF&&error==0&&closed==0,"input changed or read failed");return wire;
}
void fresh_directory(const fs::path& p){require(fs::create_directory(p),"output directory must not already exist");}
struct FixtureResult{std::size_t bytes;std::uint64_t checksum;};
FixtureResult fixture(const fs::path& output,std::size_t count,std::size_t* terminal_count=nullptr) {
    std::optional<n3::Phase> n3_phase;std::optional<n3::Scope> n3_owner;n3_phase.emplace("fixture.build");
    TerminalEvent completion(terminal_count);
    require(!fs::exists(output),"fixture output already exists");const fs::path pending=output.string()+".pending";require(!fs::exists(pending),"fixture pending output already exists");
    require(count<=(XBufferCore::MAX_PAYLOAD-chunk_bytes)/4,"fixture element count too large");const auto maximum=chunk_bytes+count*4;
    auto owner=Buffer::create(4096,XBuffer::maximum_capacity(maximum).with_growth(XGrowPolicy::Automatic));n3_owner.emplace(owner.data());auto& root=owner.root();
    root.marker=input_marker;root.label=input_label;n3_phase.emplace("fixture.reserve");root.values.reserve(count);for(std::size_t i=0;i<count;++i)root.values.push_back(fixture_value(i));
    verify_values(root,false,count);const auto checksum=semantic_checksum(root);n3_phase.emplace("fixture.save");const auto wire=owner.save_verified();
    bool created=false;n3_phase.emplace("fixture.readback_verify");try{write_exclusive(pending,wire,&created);require(read_file(pending,maximum)==wire,"fixture readback differs");
        auto verified=Buffer::load_verified(wire,XBuffer::maximum_capacity(maximum));verify_values(verified.root(),false,count);fs::create_hard_link(pending,output);
    }catch(...){if(created){std::error_code ignored;fs::remove(pending,ignored);}throw;}
    fs::remove(pending);n3_phase.emplace("fixture.teardown");return {wire.size(),checksum};
}

struct Sample {
    std::uint64_t total=0,request_send=0,server_receive=0,server_load=0,process=0,save=0,server_release=0,response_send=0,response_receive=0,client_load=0,client_validate=0,client_release=0,server_completion_wait=0;
    std::size_t input_bytes=0,output_bytes=0;
    std::string output;
    IOProgress server_io,client_io;
    std::size_t terminal_count=0;
    std::uint64_t n3_server_saved_size=0,n3_server_saved_capacity=0,n3_client_output_capacity=0;
    bool n3_server_strings_released=false;
};
struct ServerResult {IOProgress io;std::uint64_t receive=0,load=0,process=0,save=0,release=0,send=0;std::uint64_t n3_saved_size=0,n3_saved_capacity=0;bool n3_strings_released=false;};
ServerResult serve_one(int fd,const vm_network::Limits& limits,std::size_t timeout,std::size_t count,const XBufferPool& pool,std::size_t chunk=chunk_bytes,const std::string& n3_cycle="unsupported") {
    std::optional<n3::Phase> n3_phase;std::optional<n3::Scope> n3_owner;n3_phase.emplace(("server."+n3_cycle+".receive").c_str());
    ServerResult result;Deadline deadline(timeout);auto point=Clock::time_point{};if constexpr(diagnostic)point=Clock::now();
    auto wire=receive_frame(fd,limits,deadline,result.io,chunk);if constexpr(diagnostic)result.receive=elapsed(point);
    std::string saved;
    {
        if constexpr(diagnostic)point=Clock::now();
        n3_phase.emplace(("server."+n3_cycle+".load").c_str());auto owner=Buffer::load_verified(wire,XBuffer::maximum_capacity(limits.owner_capacity).with_pool(pool));n3_owner.emplace(owner.data());if constexpr(diagnostic)result.load=elapsed(point);
        result.io.published=true;n3_phase.emplace(("server."+n3_cycle+".process").c_str());if constexpr(diagnostic)point=Clock::now();process(owner.root(),count);if constexpr(diagnostic)result.process=elapsed(point);
        n3_phase.emplace(("server."+n3_cycle+".save").c_str());if constexpr(diagnostic)point=Clock::now();saved=owner.save_verified();result.n3_saved_size=saved.size();result.n3_saved_capacity=saved.capacity();if constexpr(diagnostic)result.save=elapsed(point);
        require(saved.size()<=limits.max_wire_bytes,"saved response exceeds max-wire");n3_phase.emplace(("server."+n3_cycle+".release").c_str());if constexpr(diagnostic)point=Clock::now();
    }
    if constexpr(diagnostic)result.release=elapsed(point);
    // saved owns the immutable bytes; the source owner is already released.
    n3_owner.reset();n3_phase.emplace(("server."+n3_cycle+".send").c_str());if constexpr(diagnostic)point=Clock::now();send_exact(fd,saved,deadline,result.io,chunk);if constexpr(diagnostic)result.send=elapsed(point);n3_phase.emplace(("server."+n3_cycle+".strings_release").c_str());return result;
}
// A persistent worker makes thread setup an explicit run-level preparation.
class Worker {
    int fd_;vm_network::Limits limits_;std::size_t timeout_,count_,chunk_;XBufferPool pool_;
    std::mutex mutex_;std::condition_variable changed_;bool stop_=false,task_=false,ready_=false,done_=false;
    ServerResult result_;std::exception_ptr error_;const bool n3_has_warmup_;std::thread thread_;
    void loop(){std::size_t n3_ordinal=0;for(;;){{std::unique_lock lock(mutex_);changed_.wait(lock,[&]{return stop_||task_;});if(stop_)return;task_=false;ready_=true;changed_.notify_all();}
        try{const auto n3_cycle=n3_net::cycle_name(n3_has_warmup_,n3_ordinal++);result_=serve_one(fd_,limits_,timeout_,count_,pool_,chunk_,n3_cycle);result_.n3_strings_released=true;}catch(...){error_=std::current_exception();::shutdown(fd_,SHUT_RDWR);}
        {std::lock_guard lock(mutex_);done_=true;changed_.notify_all();}}}
public:
    Worker(int fd,vm_network::Limits limits,std::size_t timeout,std::size_t count,XBufferPool pool,std::size_t chunk,bool n3_has_warmup=false):fd_(fd),limits_(limits),timeout_(timeout),count_(count),chunk_(chunk),pool_(std::move(pool)),n3_has_warmup_(n3_has_warmup),thread_([this]{loop();}){}
    void start(){std::unique_lock lock(mutex_);require(!task_,"worker already has request");ready_=done_=false;result_={};error_=nullptr;task_=true;changed_.notify_all();changed_.wait(lock,[&]{return ready_;});}
    ServerResult finish(){std::unique_lock lock(mutex_);changed_.wait(lock,[&]{return done_;});if(error_)std::rethrow_exception(error_);return result_;}
    ~Worker(){{std::lock_guard lock(mutex_);stop_=true;changed_.notify_all();}::shutdown(fd_,SHUT_RDWR);if(thread_.joinable())thread_.join();}
};
Sample exchange(Connection& connection,Worker& worker,const std::string& request,const vm_network::Limits& limits,std::size_t timeout,std::size_t count,const XBufferPool& client_pool,std::size_t chunk=chunk_bytes,const std::string& n3_cycle="unsupported") {
    std::optional<n3::Phase> n3_phase;std::optional<n3::Scope> n3_owner;n3_phase.emplace(("client."+n3_cycle+".send").c_str());
    worker.start();Sample sample;sample.input_bytes=request.size();std::optional<Buffer> verified;IOProgress progress;Deadline deadline(timeout);
    const auto start=Clock::now();auto point=start;send_exact(connection.client.fd,request,deadline,progress,chunk);if constexpr(diagnostic)sample.request_send=elapsed(point);
    n3_phase.emplace(("client."+n3_cycle+".receive").c_str());if constexpr(diagnostic)point=Clock::now();sample.output=receive_frame(connection.client.fd,limits,deadline,progress,chunk);sample.output_bytes=sample.output.size();sample.n3_client_output_capacity=sample.output.capacity();if constexpr(diagnostic)sample.response_receive=elapsed(point);
    n3_phase.emplace(("client."+n3_cycle+".load").c_str());if constexpr(diagnostic)point=Clock::now();verified.emplace(Buffer::load_verified(sample.output,XBuffer::maximum_capacity(limits.owner_capacity).with_pool(client_pool)));n3_owner.emplace(verified->data());if constexpr(diagnostic)sample.client_load=elapsed(point);
    n3_phase.emplace(("client."+n3_cycle+".validate").c_str());if constexpr(diagnostic)point=Clock::now();verify_values(verified->root(),true,count);deadline.check();if constexpr(diagnostic)sample.client_validate=elapsed(point);sample.total=elapsed(start);
    n3_phase.emplace(("client."+n3_cycle+".release").c_str());point=Clock::now();verified.reset();sample.client_release=elapsed(point);n3_owner.reset();n3_phase.emplace(("client."+n3_cycle+".server_wait").c_str());point=Clock::now();const auto server=worker.finish();sample.server_completion_wait=elapsed(point);sample.n3_server_saved_size=server.n3_saved_size;sample.n3_server_saved_capacity=server.n3_saved_capacity;sample.n3_server_strings_released=server.n3_strings_released;
    sample.server_receive=server.receive;sample.server_load=server.load;sample.process=server.process;sample.save=server.save;sample.server_release=server.release;sample.response_send=server.send;
    progress.published=true;
    require(server.io.published&&server.io.read==request.size()&&server.io.written==sample.output.size()&&progress.read==sample.output.size()&&progress.written==request.size()&&progress.allocations==1,"endpoint completion accounting differs");
    sample.server_io=server.io;sample.client_io=progress;++sample.terminal_count;return sample;
}

struct RunResult {
    std::string pool;vm_network::Limits limits;std::size_t count=0,timeout=0;std::uint64_t checksum=0,prepare=0,connection=0,warmup=0,cold_clear=0,final_clear=0,client_release=0,server_completion_wait=0;
    XBufferPool::Stats server_before{},server_after{},client_before{},client_after{};std::vector<Sample> samples;std::optional<n3_net::Cycle> n3_warm;XBufferPool::Stats n3_server_final{},n3_client_final{};std::uint64_t n3_ledger_bytes=0,n3_ledger_frames=0,n3_ledger_peak=0;
};
class RunReservation {
    vm_network::ResourceLedger& ledger_;std::size_t bytes_;bool held_=false;
public:
    inline static std::size_t active_bytes=0,active_queue_frames=0;
    explicit RunReservation(vm_network::ResourceLedger& ledger):ledger_(ledger),bytes_(4*ledger.limits().max_wire_bytes){
        require(ledger_.acquire_queue(),"request queue admission failed");
        if(!ledger_.acquire_wire(bytes_)){ledger_.release_queue();throw Failure("inflight_budget","four-buffer logical admission failed");}held_=true;active_bytes+=bytes_;++active_queue_frames;
    }
    void release() noexcept {if(held_){ledger_.release_wire(bytes_);ledger_.release_queue();active_bytes-=bytes_;--active_queue_frames;held_=false;}}
    ~RunReservation(){release();}
};
void validate_options(const vm_network::Limits& limits,std::size_t iterations,std::size_t timeout) {
    require(iterations>0&&iterations<=100000,"iterations must be in 1..100000");
    require(limits.max_wire_bytes>=4096&&limits.max_wire_bytes<=XBufferCore::MAX_PAYLOAD+H,"max-wire outside 4096..MAX_PAYLOAD+88");
    require(limits.owner_capacity>=4096&&limits.owner_capacity<=XBufferCore::MAX_PAYLOAD,"owner-capacity outside 4096..MAX_PAYLOAD");
    require(limits.max_inflight_bytes>=4*limits.max_wire_bytes&&limits.max_inflight_bytes<=8*(static_cast<std::size_t>(XBufferCore::MAX_PAYLOAD)+H),"max-inflight must cover four max-wire owning buffers and remain within fixed limit");
    require(timeout>0&&timeout<=600000,"timeout-ms outside 1..600000");
}
RunResult run(const fs::path& input,const fs::path& directory,std::string mode,std::size_t iterations,vm_network::Limits limits,std::size_t timeout,std::size_t chunk=chunk_bytes) {
    std::optional<n3::Phase> n3_phase;n3_phase.emplace("init.input");
    validate_options(limits,iterations,timeout);require(mode=="none"||mode=="cold"||mode=="warm","invalid pool mode");
    vm_network::ResourceLedger ledger(limits);RunReservation reservation(ledger);
    auto request=read_file(input,limits.max_wire_bytes);XWireHeaderV1 header{};std::memcpy(&header,request.data(),H);
    const auto admission=vm_network::admit_header(header,limits,type_id,schema_hash);require(static_cast<bool>(admission)&&admission.wire_bytes==request.size(),"input header/length admission failed");
    RunResult result;result.pool=mode;result.limits=limits;result.timeout=timeout;
    n3_phase.emplace("init.preflight");{std::optional<n3::Scope> n3_owner;auto preflight=Buffer::load_verified(request,XBuffer::maximum_capacity(limits.owner_capacity));n3_owner.emplace(preflight.data());result.count=preflight.root().values.size();verify_values(preflight.root(),false,result.count);process(preflight.root(),result.count);result.checksum=semantic_checksum(preflight.root());}
    fresh_directory(directory);auto point=Clock::now();XBufferPool server_pool,client_pool;
    if(mode!="none"){const auto budget=limits.owner_capacity+chunk_bytes;n3_phase.emplace("init.pool.server");server_pool=XBufferPool({1,budget,budget});n3_phase.emplace("init.pool.client");client_pool=XBufferPool({1,budget,budget});}
    result.prepare=elapsed(point);n3_phase.emplace("init.connection");point=Clock::now();auto connection=connect_loopback(timeout);
    Worker worker(connection.server.fd,limits,timeout,result.count,server_pool,chunk,mode=="warm");result.connection=elapsed(point);
    if(mode=="warm"){point=Clock::now();{auto n3_warm_sample=exchange(connection,worker,request,limits,timeout,result.count,client_pool,chunk,"warm");result.n3_warm=n3_net::observe_cycle(n3_warm_sample,"warm");n3_phase.emplace("warm.output_archive");write_exclusive(directory/"n3-warm-output.wire",n3_warm_sample.output);n3_phase.emplace("warm.output_release");}result.warmup=elapsed(point);}
    n3_phase.emplace("init.pool.stats");result.server_before=server_pool.stats();result.client_before=client_pool.stats();result.samples.reserve(iterations);
    for(std::size_t i=0;i<iterations;++i){if(mode=="cold"){n3_phase.emplace(("cold.clear."+std::to_string(i)).c_str());point=Clock::now();server_pool.clear();client_pool.clear();result.cold_clear+=elapsed(point);}
        auto sample=exchange(connection,worker,request,limits,timeout,result.count,client_pool,chunk,"work."+std::to_string(i));result.client_release+=sample.client_release;result.server_completion_wait+=sample.server_completion_wait;
        n3_phase.emplace(("work."+std::to_string(i)+".output_archive").c_str());write_exclusive(directory/("output-"+std::to_string(i)+".wire"),sample.output);
        n3_phase.emplace(("work."+std::to_string(i)+".output_release").c_str());std::string{}.swap(sample.output);result.samples.push_back(std::move(sample));}
    n3_phase.emplace("before_final.stats");result.server_after=server_pool.stats();result.client_after=client_pool.stats();
    const auto hits=result.server_after.hits+result.client_after.hits-result.server_before.hits-result.client_before.hits;
    const auto misses=result.server_after.misses+result.client_after.misses-result.server_before.misses-result.client_before.misses;
    require(hits==(mode=="warm"?2*iterations:0)&&misses==(mode=="cold"?2*iterations:0),"pool hit/miss expectation failed");
    n3_phase.emplace("final.clear");point=Clock::now();server_pool.clear();client_pool.clear();result.final_clear=elapsed(point);n3_phase.emplace("diag.pool.check");result.n3_server_final=server_pool.stats();result.n3_client_final=client_pool.stats();
    n3_phase.emplace("request.release");std::string{}.swap(request);reservation.release();require(ledger.inflight_bytes()==0&&ledger.queue_frames()==0&&ledger.peak_inflight_bytes()==4*limits.max_wire_bytes,"logical run reservation not restored");result.n3_ledger_bytes=ledger.inflight_bytes();result.n3_ledger_frames=ledger.queue_frames();result.n3_ledger_peak=ledger.peak_inflight_bytes();n3_phase.emplace("run.teardown");return result;
}
std::string phase(std::uint64_t value){if constexpr(diagnostic)return std::to_string(value);else return "null";}
void prefix(std::string_view command){std::cout<<"{\"schema_version\":1,\"contract_version\":\"1.0.0\",\"status\":\"passed\",\"command\":"<<quoted(command)<<",\"diagnostic\":"<<(diagnostic?"true":"false")<<",\"timing_evidence\":false";}
void sample_json(const Sample& s){std::cout<<"{\"end_to_end_ns\":"<<s.total<<",\"request_send_ns\":"<<phase(s.request_send)<<",\"server_receive_ns\":"<<phase(s.server_receive)<<",\"server_load_verified_ns\":"<<phase(s.server_load)<<",\"server_process_ns\":"<<phase(s.process)<<",\"server_save_verified_ns\":"<<phase(s.save)<<",\"server_owner_release_ns\":"<<phase(s.server_release)<<",\"response_send_ns\":"<<phase(s.response_send)<<",\"response_receive_ns\":"<<phase(s.response_receive)<<",\"client_load_verified_ns\":"<<phase(s.client_load)<<",\"client_validate_ns\":"<<phase(s.client_validate)<<",\"client_owner_release_outside_ns\":"<<s.client_release<<",\"server_completion_wait_outside_ns\":"<<s.server_completion_wait<<",\"input_bytes\":"<<s.input_bytes<<",\"output_bytes\":"<<s.output_bytes<<'}';}
void run_json(const RunResult& r){prefix("run");std::uint64_t total=0;for(const auto& s:r.samples)total+=s.total;
    const auto hits=r.server_after.hits+r.client_after.hits-r.server_before.hits-r.client_before.hits;
    const auto misses=r.server_after.misses+r.client_after.misses-r.server_before.misses-r.client_before.misses;
    std::cout<<",\"pool\":"<<quoted(r.pool)<<",\"iterations\":"<<r.samples.size()<<",\"element_count\":"<<r.count<<",\"semantic_checksum\":"<<quoted(std::to_string(r.checksum))<<",\"input_bytes_per_iteration\":"<<r.samples.front().input_bytes<<",\"output_bytes_per_iteration\":"<<r.samples.front().output_bytes
        <<",\"max_wire_bytes\":"<<r.limits.max_wire_bytes<<",\"owner_capacity_bytes\":"<<r.limits.owner_capacity<<",\"max_inflight_bytes\":"<<r.limits.max_inflight_bytes<<",\"reserved_inflight_peak_bytes\":"<<4*r.limits.max_wire_bytes<<",\"connection_count\":1,\"max_inflight_requests\":1,\"timeout_ms\":"<<r.timeout<<",\"end_to_end_ns\":"<<total
        <<",\"pool_prepare_outside_ns\":"<<r.prepare<<",\"connection_setup_outside_ns\":"<<r.connection<<",\"warmup_outside_ns\":"<<r.warmup<<",\"cold_clear_outside_ns\":"<<r.cold_clear<<",\"final_clear_outside_ns\":"<<r.final_clear<<",\"client_owner_release_outside_ns\":"<<r.client_release<<",\"server_completion_wait_outside_ns\":"<<r.server_completion_wait
        <<",\"pool_measured_hits\":"<<hits<<",\"pool_measured_misses\":"<<misses<<",\"pool_warmup_hits\":"<<r.server_before.hits+r.client_before.hits<<",\"pool_warmup_misses\":"<<r.server_before.misses+r.client_before.misses
        <<",\"pool_cached_blocks_before_final_clear\":"<<r.server_after.cached_blocks+r.client_after.cached_blocks<<",\"pool_cached_reserved_bytes_before_final_clear\":"<<r.server_after.cached_reserved_bytes+r.client_after.cached_reserved_bytes<<",\"pool_cached_accessible_bytes_before_final_clear\":"<<r.server_after.cached_accessible_bytes+r.client_after.cached_accessible_bytes<<",\"samples\":[";
    for(std::size_t i=0;i<r.samples.size();++i){if(i)std::cout<<',';sample_json(r.samples[i]);}std::cout<<"],\"output_files\":[";
    for(std::size_t i=0;i<r.samples.size();++i){if(i)std::cout<<',';std::cout<<quoted("output-"+std::to_string(i)+".wire");}std::cout<<"]}\n";
}

std::string invalid_graph(const std::string& valid) {
    XWireHeaderV1 h{};std::memcpy(&h,valid.data(),H);auto bad=valid;
    const auto offset=H+h.root_offset+offsetof(IoReferenceRoot,values)+offsetof(XVector<std::uint32_t>,size_);
    require(offset+sizeof(std::uint32_t)<=bad.size(),"graph mutation offset");const std::uint32_t value=std::numeric_limits<std::uint32_t>::max();std::memcpy(bad.data()+offset,&value,sizeof(value));return bad;
}
bool graph_verifier(const std::string& wire,std::size_t capacity) {
    try{auto owner=Buffer::load_verified(wire,XBuffer::maximum_capacity(capacity));return true;}
    catch(const std::exception& error){if(std::string_view(error.what())=="wire validation failed: XFixedVector size exceeds capacity")return false;throw;}
}
using Record=vm_network::TestRecord;
Record checked_record(std::string id,std::string parent,std::string expected,std::string actual,const IOProgress& progress,std::size_t read,std::size_t written,std::size_t allocations,bool published,std::size_t terminals=1) {
    require(expected==actual,"negative failed for unexpected cause: "+id+": "+actual);
    require(progress.read==read&&progress.written==written&&progress.allocations==allocations&&progress.published==published&&progress.terminal_count==terminals,"negative progress/publication/allocation/terminal differs: "+id);
    Record r;r.id=std::move(id);r.parent_id=std::move(parent);r.expected_reason=std::move(expected);r.actual_reason=std::move(actual);
    r.read_bytes=progress.read;r.expected_read_bytes=read;r.written_bytes=progress.written;r.expected_written_bytes=written;r.receive_allocations=progress.allocations;r.expected_receive_allocations=allocations;r.published=progress.published;r.expected_published=published;r.terminal_count=progress.terminal_count;r.expected_terminal_count=terminals;
    r.active_bytes=RunReservation::active_bytes;r.active_queue_frames=RunReservation::active_queue_frames;require(r.active_bytes==0&&r.active_queue_frames==0,"native logical credits not restored");
    return r;
}
struct SmokeResult {std::vector<std::string> groups;std::vector<Record> records;std::size_t fixture_bytes=0,backpressure_bytes=0;};
void smoke(const fs::path& directory) {
    fresh_directory(directory);constexpr std::size_t maximum=1024*1024;vm_network::Limits limits{maximum,maximum,4*maximum,1};
    const auto file=directory/"input.wire";const auto made=fixture(file,257);const auto valid=read_file(file,maximum),bad=invalid_graph(valid);SmokeResult result;result.fixture_bytes=made.bytes;
    result.groups=vm_network::run_frame_model_tests(valid,bad,limits,type_id,schema_hash,[&](const std::string& w){return graph_verifier(w,maximum);},&result.records);
    auto add=[&](const std::string& id,const IOProgress& p,std::string reason="ok",std::size_t terminals=1){result.groups.push_back(id);result.records.push_back(checked_record(id,id,reason,reason,p,p.read,p.written,p.allocations,p.published,terminals));};
    auto observed_run=[&](const RunResult& r){IOProgress p;p.published=true;for(const auto& s:r.samples){p.read+=s.server_io.read+s.client_io.read;p.written+=s.server_io.written+s.client_io.written;p.allocations+=s.server_io.allocations+s.client_io.allocations;p.published=p.published&&s.server_io.published&&s.client_io.published;p.terminal_count+=s.terminal_count;}return p;};
    for(const std::string mode:{"none","cold","warm"}) {
        const auto r=run(file,directory/mode,mode,2,limits,2000);require(r.checksum!=0&&r.samples.size()==2,"pool semantic completion");
        auto p=observed_run(r);require(p.read==4*valid.size()&&p.written==4*r.samples[0].output_bytes&&p.allocations==4,"observed pool roundtrip progress");add("tcp.pool."+mode,p,"ok",r.samples.size());
    }
    {
        const auto r=run(file,directory/"fragmented","none",2,limits,2000,7);require(r.samples.size()==2,"positive short I/O did not complete");
        auto p=observed_run(r);require(p.read==4*valid.size()&&p.written==4*r.samples[0].output_bytes&&p.allocations==4,"observed short roundtrip progress");add("tcp.fragmented_short_io",p,"ok",r.samples.size());
    }
    {
        auto connection=connect_loopback(2000);IOProgress sent,p;Deadline deadline(2000);send_exact(connection.client.fd,valid+valid,deadline,sent);
        for(int i=0;i<2;++i){const auto wire=receive_frame(connection.server.fd,limits,deadline,p);auto owner=Buffer::load_verified(wire,XBuffer::maximum_capacity(maximum));verify_values(owner.root(),false,257);p.published=true;}
        require(p.read==2*valid.size()&&p.allocations==2,"contiguous stream frame boundary");add("tcp.contiguous_frames",p,"ok",2);
    }
    {
        auto connection=connect_loopback(2000);IOProgress sent,p;Deadline deadline(2000);send_exact(connection.client.fd,valid,deadline,sent);require(shutdown(connection.client.fd,SHUT_WR)==0,"half-close failed");
        const auto wire=receive_frame(connection.server.fd,limits,deadline,p);auto owner=Buffer::load_verified(wire,XBuffer::maximum_capacity(maximum));verify_values(owner.root(),false,257);p.published=true;
        deadline.wait(connection.server.fd,POLLIN);char byte=0;require(recv(connection.server.fd,&byte,1,0)==0&&p.read==valid.size(),"EOF after exact frame affected framing");add("tcp.eof_after_frame",p);
    }
    auto reject_receive=[&](const std::string& id,std::string_view wire,std::string expected,std::size_t read,std::size_t allocations,bool verify_graph=false){
        auto connection=connect_loopback(2000);IOProgress sent,p;Deadline deadline(2000);send_exact(connection.client.fd,wire,deadline,sent);require(shutdown(connection.client.fd,SHUT_WR)==0,"negative half-close failed");std::string reason;
        try{auto received=receive_frame(connection.server.fd,limits,deadline,p);if(verify_graph){require(!graph_verifier(received,maximum),"graph negative unexpectedly loaded");reason="invalid_graph";}else reason="unexpected_success";}
        catch(const Failure& error){reason=error.reason;}
        auto record=checked_record(id,id,expected,reason,p,read,0,allocations,false);result.groups.push_back(id);result.records.push_back(std::move(record));
    };
    reject_receive("tcp.eof_header",std::string_view(valid).substr(0,7),"eof_header",7,0);
    reject_receive("tcp.eof_payload",std::string_view(valid).substr(0,H+7),"eof_payload",H+7,1);
    auto invalid_header=valid;invalid_header[0]^=0x55;
    reject_receive("tcp.invalid_header_no_allocation",std::string_view(invalid_header).substr(0,H),"bad_magic",H,0);
    reject_receive("tcp.invalid_graph_no_publication",bad,"invalid_graph",bad.size(),1,true);
    {
        // A failed verified load may reuse a dirty pooled mapping on the next
        // complete load. Full value checks prove complete visible replacement.
        XBufferPool pool({1,maximum+chunk_bytes,maximum+chunk_bytes});bool expected=false;
        try{auto owner=Buffer::load_verified(bad,XBuffer::maximum_capacity(maximum).with_pool(pool));}
        catch(const std::exception& e){expected=std::string_view(e.what())=="wire validation failed: XFixedVector size exceeds capacity";}
        require(expected,"failed-load retry expected graph reason");auto connection=connect_loopback(2000);IOProgress sent,p;Deadline deadline(2000);send_exact(connection.client.fd,valid,deadline,sent);auto wire=receive_frame(connection.server.fd,limits,deadline,p);
        auto owner=Buffer::load_verified(wire,XBuffer::maximum_capacity(maximum).with_pool(pool));verify_values(owner.root(),false,257);p.published=true;
        result.records.push_back(checked_record("tcp.invalid_graph.retry_full_load","tcp.invalid_graph_no_publication","ok","ok",p,valid.size(),0,1,true));
    }
    {
        auto connection=connect_loopback(2000);IOProgress sent,p;Deadline deadline(2000);send_exact(connection.client.fd,valid,deadline,sent);auto wire=receive_frame(connection.server.fd,limits,deadline,p);
        XBufferPool pool({1,maximum+chunk_bytes,maximum+chunk_bytes});auto live=Buffer::load_verified(wire,XBuffer::maximum_capacity(maximum).with_pool(pool));pool.clear();verify_values(live.root(),false,257);p.published=true;
        require(pool.stats().cached_blocks==0,"pool clear unexpectedly caches live owner");result.records.push_back(checked_record("tcp.pool.warm.clear_live_owner","tcp.pool.warm","ok","ok",p,valid.size(),0,1,true));
    }
    {
        auto connection=connect_loopback(2000);IOProgress sent,p;Deadline deadline(2000);send_exact(connection.client.fd,valid,deadline,sent);std::string reason;
        try{auto wire=receive_frame(connection.server.fd,limits,deadline,p);auto owner=Buffer::load_verified(wire,XBuffer::maximum_capacity(maximum));p.published=true;process(owner.root(),258);reason="unexpected_success";}
        catch(const Failure& e){require(std::string_view(e.what())=="input count changed since preflight","processing negative failed elsewhere");reason="processing_failed";}
        require(shutdown(connection.server.fd,SHUT_RDWR)==0,"processing failure must close self-owned connection");deadline.wait(connection.client.fd,POLLIN);char byte;const auto n=recv(connection.client.fd,&byte,1,0);require(n==0,"processing failure emitted successful response");
        result.records.push_back(checked_record("tcp.processing_failure.no_send","tcp.fragmented_short_io","processing_failed",reason,p,valid.size(),0,1,true));
    }
    {
        // Real socket with a deliberately truncated response, not a model event.
        auto connection=connect_loopback(2000);IOProgress sent,p;Deadline deadline(2000);send_exact(connection.server.fd,std::string_view(valid).substr(0,H+7),deadline,sent);shutdown(connection.server.fd,SHUT_WR);std::string reason;
        try{(void)receive_frame(connection.client.fd,limits,deadline,p);reason="unexpected_success";}catch(const Failure& e){reason=e.reason;}
        result.groups.push_back("tcp.response_eof_payload");result.records.push_back(checked_record("tcp.response_eof_payload","tcp.response_eof_payload","eof_payload",reason,p,H+7,0,1,false));
    }
    {
        auto connection=connect_loopback(2000);IOProgress p;std::string reason;try{(void)receive_frame(connection.server.fd,limits,Deadline(30),p);reason="unexpected_success";}catch(const Failure& e){reason=e.reason;}
        result.groups.push_back("tcp.deadline_header");result.records.push_back(checked_record("tcp.deadline_header","tcp.deadline_header","timeout",reason,p,0,0,0,false));
    }
    {
        auto connection=connect_loopback(2000);IOProgress sent,received;Deadline deadline(2000);vm_network::ResourceLedger ledger(limits);std::string reason;
        try{RunReservation reservation(ledger);send_exact(connection.client.fd,std::string_view(valid).substr(0,7),deadline,sent);char bytes[7];read_exact(connection.server.fd,bytes,7,deadline,received,"eof_header");
            linger reset{1,0};require(setsockopt(connection.server.fd,SOL_SOCKET,SO_LINGER,&reset,sizeof(reset))==0,"RST option failed");connection.server.close();
            deadline.wait(connection.client.fd,POLLIN);char byte;require(recv(connection.client.fd,&byte,1,0)<=0,"disconnect expected before remaining send");
            send_exact(connection.client.fd,std::string_view(valid).substr(7),deadline,sent);reason="unexpected_success";
        }catch(const Failure& e){reason=e.reason;++sent.terminal_count;}
        require(received.read==7&&ledger.inflight_bytes()==0&&ledger.queue_frames()==0&&ledger.peak_inflight_bytes()==4*limits.max_wire_bytes,"partial send disconnect progress/credit restore");result.groups.push_back("tcp.partial_send_disconnect");result.records.push_back(checked_record("tcp.partial_send_disconnect","tcp.partial_send_disconnect","write_error",reason,sent,0,7,0,false));
    }
    {
        // Guarantee a real EAGAIN before allowing the slow reader to drain.
        auto connection=connect_loopback(4000);int small=4096;require(setsockopt(connection.server.fd,SOL_SOCKET,SO_SNDBUF,&small,sizeof(small))==0,"send-buffer option failed");
        const auto large_file=directory/"backpressure.wire";(void)fixture(large_file,524288);auto bytes=read_file(large_file,4*maximum);result.backpressure_bytes=bytes.size();
        IOProgress sent,received;Deadline deadline(4000);std::size_t done=0;bool blocked=false;
        while(done<bytes.size()){deadline.check();const auto n=send(connection.server.fd,bytes.data()+done,bytes.size()-done,0);if(n>0){done+=n;sent.written+=n;continue;}if(n<0&&errno==EINTR)continue;require(n<0&&(errno==EAGAIN||errno==EWOULDBLOCK),"unexpected slow-writer status");blocked=true;break;}
        require(blocked&&done>0&&done<bytes.size(),"slow reader did not produce backpressure");std::exception_ptr reader_error;
        std::thread reader([&]{try{vm_network::Limits large_limits{4*maximum,4*maximum,16*maximum,1};auto sink=receive_frame(connection.client.fd,large_limits,deadline,received,1024);require(sink==bytes,"slow-reader data mismatch");auto owner=Buffer::load_verified(sink,XBuffer::maximum_capacity(4*maximum));verify_values(owner.root(),false,524288);received.published=true;}catch(...){reader_error=std::current_exception();shutdown(connection.client.fd,SHUT_RDWR);}});
        try{send_exact(connection.server.fd,std::string_view(bytes).substr(done),deadline,sent);reader.join();}catch(...){shutdown(connection.server.fd,SHUT_RDWR);reader.join();throw;}
        if(reader_error)std::rethrow_exception(reader_error);require(sent.written==bytes.size()&&received.read==bytes.size(),"backpressure restoration bytes");
        IOProgress p{received.read,sent.written,received.allocations,sent.waits,received.published,received.terminal_count};add("tcp.slow_reader_backpressure",p);
    }
    {
        IOProgress p;std::string reason;try{(void)fixture(file,258,&p.terminal_count);reason="unexpected_success";}catch(const Failure& e){require(std::string_view(e.what())=="fixture output already exists","existing fixture failure cause");reason="existing_output";}
        require(read_file(file,maximum)==valid,"existing fixture changed");add("fixture.existing_output",p,reason);require(reason=="existing_output","duplicate fixture accepted");
    }
    {
        const auto output=directory/"protected.wire";const fs::path pending=output.string()+".pending";write_exclusive(pending,"sentinel");std::string reason;
        IOProgress p;try{(void)fixture(output,1,&p.terminal_count);reason="unexpected_success";}catch(const Failure& e){require(std::string_view(e.what())=="fixture pending output already exists","pending fixture failure cause");reason="existing_pending";}
        require(!fs::exists(output)&&fs::file_size(pending)==8,"existing pending output changed");char bytes[8];std::FILE* sentinel=std::fopen(pending.string().c_str(),"rb");require(sentinel!=nullptr,"cannot read protected sentinel");const auto read=std::fread(bytes,1,8,sentinel);const auto close=std::fclose(sentinel);require(read==8&&close==0&&std::string_view(bytes,8)=="sentinel","protected pending content changed");add("fixture.existing_pending",p,reason);require(reason=="existing_pending","pending accepted");
    }
    require(result.groups.size()==34,"smoke group inventory count");require(Socket::live_count==0,"owned socket leaked after smoke cases");prefix("smoke");std::cout<<",\"checks\":"<<result.groups.size()<<",\"owned_sockets_after_cases\":0,\"fixture_frame_bytes\":"<<result.fixture_bytes<<",\"invalid_graph_frame_bytes\":"<<bad.size()<<",\"backpressure_frame_bytes\":"<<result.backpressure_bytes<<",\"case_ids\":[";
    for(std::size_t i=0;i<result.groups.size();++i){if(i)std::cout<<',';std::cout<<quoted(result.groups[i]);}
    std::cout<<"],\"cases\":[";for(std::size_t i=0;i<result.groups.size();++i){if(i)std::cout<<',';const auto& id=result.groups[i];std::size_t n=0;for(const auto& r:result.records)if(r.parent_id==id)++n;std::cout<<"{\"id\":"<<quoted(id)<<",\"scope\":"<<quoted(id.starts_with("model.")?"injected_model":id.starts_with("tcp.")?"actual_tcp":"filesystem")<<",\"status\":\"passed\",\"subcase_count\":"<<n<<"}";}
    std::cout<<"],\"subcases\":[";for(std::size_t i=0;i<result.records.size();++i){if(i)std::cout<<',';std::cout<<vm_network::test_record_json(result.records[i]);}std::cout<<"]}\n";
}

std::size_t number(const std::string& text){require(!text.empty()&&text.find_first_not_of("0123456789")==std::string::npos,"expected unsigned decimal integer");std::size_t used=0;const auto n=std::stoull(text,&used);require(used==text.size()&&n<=std::numeric_limits<std::size_t>::max(),"integer out of range");return n;}
} // namespace
int main(int argc,char** argv){std::string command=argc>1?argv[1]:"";try{require(argc>=2&&argc%2==0,"usage: fixture|smoke|run with contract.json options");std::map<std::string,std::string> options;for(int i=2;i<argc;i+=2)require(options.emplace(argv[i],argv[i+1]).second,"duplicate option");
    auto take=[&](const std::string& key){auto it=options.find(key);require(it!=options.end(),"missing option: "+key);auto value=it->second;options.erase(it);return value;};
    if(command=="fixture"){const auto output=take("--output");const auto count=number(take("--elements"));require(options.empty(),"unexpected option");const auto result=fixture(output,count);n3_net::dump_fixture(output,count,result);prefix(command);std::cout<<",\"element_count\":"<<count<<",\"frame_bytes\":"<<result.bytes<<",\"semantic_checksum\":"<<quoted(std::to_string(result.checksum))<<"}\n";}
    else if(command=="smoke"){const auto output=take("--out-dir");require(options.empty(),"unexpected option");smoke(output);}
    else if(command=="run"){const auto input=take("--input"),output=take("--out-dir"),pool=take("--pool");const auto iterations=number(take("--iterations")),wire=number(take("--max-wire")),owner=number(take("--owner-capacity")),inflight=number(take("--max-inflight-bytes")),timeout=number(take("--timeout-ms"));require(options.empty(),"unexpected option");const auto n3_result=run(input,output,pool,iterations,{wire,owner,inflight,1},timeout);n3_net::dump_run(output,n3_result);run_json(n3_result);}
    else throw Failure("usage","unknown command");return 0;
}catch(const std::exception& error){const auto* failure=dynamic_cast<const Failure*>(&error);std::cout<<"{\"schema_version\":1,\"contract_version\":\"1.0.0\",\"status\":\"failed\",\"command\":"<<quoted(command)<<",\"diagnostic\":"<<(diagnostic?"true":"false")<<",\"timing_evidence\":false,\"reason\":"<<quoted(failure?failure->reason:"verification_or_runtime")<<",\"error\":"<<quoted(error.what())<<"}\n";return 2;}}

