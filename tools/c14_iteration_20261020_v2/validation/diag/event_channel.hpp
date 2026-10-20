#pragma once
// Diagnostic-only, main-thread bounded channel. Never use in bare timing builds.
#include <cerrno>
#include <charconv>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>

namespace c14_event {
struct State {
    unsigned sequence{};
    int fd{-2};
    std::uint64_t errors{}, writes{}, stdout_attempted_bytes{};
    int last_error{}, clock_status{}, mask_status{}, restore_status{};
};
inline thread_local State state{};
inline void fail(std::uint64_t bit,int error) noexcept { state.errors|=bit;state.last_error=error; }

// The inherited event pipe must already be nonblocking; never change its OFD.
inline int event_fd() noexcept {
    if(state.fd!=-2)return state.fd;
    state.fd=-1;const char* p=std::getenv("XOFFSET_DIAG_EVENT_FD");
    if(!p||!*p){fail(1,EBADF);return -1;}
    unsigned value=0;
    for(;*p;++p){if(*p<'0'||*p>'9'||value>(unsigned(INT_MAX)-unsigned(*p-'0'))/10){fail(1,EINVAL);return -1;}value=value*10+unsigned(*p-'0');}
    const auto fd=int(value);const int flags=::fcntl(fd,F_GETFL);struct stat st{};
    if(flags<0||!(flags&O_NONBLOCK)||::fstat(fd,&st)!=0||!S_ISFIFO(st.st_mode)){fail(1,EBADF);return -1;}
    state.fd=fd;return fd;
}

// One write, never retry. Preserve the caller's errno and process-wide SIGPIPE
// disposition. A previously pending SIGPIPE is never consumed. Only on EPIPE
// with no pre-existing pending SIGPIPE do we consume the newly generated one,
// using a zero-timeout wait while this thread has SIGPIPE blocked.
inline bool once(int fd,const char* bytes,std::size_t count) noexcept {
    const int saved=errno;sigset_t one{},old{},pending{};
    ::sigemptyset(&one);::sigaddset(&one,SIGPIPE);
    state.mask_status=::pthread_sigmask(SIG_BLOCK,&one,&old);
    if(state.mask_status){fail(8,state.mask_status);errno=saved;return false;}
    if(::sigpending(&pending)!=0){fail(8,errno);state.restore_status=::pthread_sigmask(SIG_SETMASK,&old,nullptr);errno=saved;return false;}
    const bool had_pipe=::sigismember(&pending,SIGPIPE)==1;
    const auto n=::write(fd,bytes,count);const int write_error=n<0?errno:0;
    if(n<0&&write_error==EPIPE&&!had_pipe){
        const timespec zero{};siginfo_t info{};
        const int got=::sigtimedwait(&one,&info,&zero);
        if(got!=SIGPIPE&&!(got<0&&errno==EAGAIN))fail(32,errno);
    }
    state.restore_status=::pthread_sigmask(SIG_SETMASK,&old,nullptr);
    if(state.restore_status)fail(64,state.restore_status);
    const bool complete=n>=0&&std::size_t(n)==count;
    if(!complete)fail(16,write_error?write_error:EIO);
    ++state.writes;errno=saved;return complete&&state.restore_status==0;
}

inline void event(std::string_view label,const std::uint64_t* pod=nullptr) noexcept {
    const int saved=errno;const auto seq=state.sequence++;
    if(seq>=64){fail(4,EOVERFLOW);errno=saved;return;}
    const int fd=event_fd();timespec stamp{};state.clock_status=::clock_gettime(CLOCK_MONOTONIC,&stamp);
    if(fd<0||state.clock_status){if(state.clock_status)fail(2,errno);errno=saved;return;}
    char bytes[128];char* p=bytes;char* const end=bytes+sizeof(bytes);bool good=true;
    auto text=[&](std::string_view v) noexcept {if(v.size()>std::size_t(end-p)){good=false;return;}for(char c:v)*p++=c;};
    auto number=[&](std::uint64_t v) noexcept {auto r=std::to_chars(p,end,v);if(r.ec!=std::errc{})good=false;else p=r.ptr;};
    text("XD1 ");number(seq);text(" ");number(std::uint64_t(stamp.tv_sec)*1000000000ULL+std::uint64_t(stamp.tv_nsec));text(" ");text(label);
    if(pod)for(unsigned i=0;i<4;++i){text(" ");number(pod[i]);}
    text("\n");if(!good)fail(4,EOVERFLOW);else once(fd,bytes,std::size_t(p-bytes));errno=saved;
}

// Outside emit only. Caller supplies one complete bounded JSON line. Changing
// child stdout's writer OFD to nonblocking prevents diagnostic logging from
// waiting for the parent; a partial write is an explicit diagnostic failure.
inline void json(std::string_view line) noexcept {
    const int saved=errno;
    if(line.size()>65536||state.stdout_attempted_bytes>1048576||line.size()>1048576-state.stdout_attempted_bytes){fail(128,EFBIG);errno=saved;return;}
    state.stdout_attempted_bytes+=line.size();const int flags=::fcntl(STDOUT_FILENO,F_GETFL);
    if(flags<0||::fcntl(STDOUT_FILENO,F_SETFL,flags|O_NONBLOCK)<0){fail(128,errno);errno=saved;return;}
    once(STDOUT_FILENO,line.data(),line.size());errno=saved;
}
inline bool good() noexcept {return state.errors==0;}
}
