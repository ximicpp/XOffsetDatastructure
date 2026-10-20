#include "receive_raw.hpp"
#include "root.hpp"
int raw_first(){return int(sizeof(xp::VerifiedReceiveToken<PackageReceiveRoot>)>0);}
