#include "receive_future.hpp"
#include "root.hpp"
int future_first(){return int(sizeof(xp::VerifiedReceiveToken<PackageReceiveRoot>)>0);}
