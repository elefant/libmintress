#ifndef NET_PORT_POOL_H
#define NET_PORT_POOL_H

#include "utl/ResourcePool.h"
#include "net/Misc.h"
#include "utl/Logger.h"

namespace net
{
    class PortPool : public utl::ResourcePool<short>, utl::Logger
    {
    public:
        enum Protocol { PROTOCOL_TCP, PROTOCOL_UDP }; 

    public:
        PortPool
            (
            Protocol aProtocol,
            short aRangeStart,
            short aRangeEnd
            );

        ~PortPool();

        bool acquire
            (
            short& aResource
            );

    private:
        Protocol iProtocol;
    };
}

#endif

