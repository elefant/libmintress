#ifndef NET_REPEATER_H
#define NET_REPEATER_H

#include <vector>
#include <set>
#include <string>

#include "TcpSocket.h"
#include "UdpSocket.h"
#include "utl/Logger.h"

#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>

namespace boost
{
    class thread;
};

namespace net
{
    class Repeater : private utl::Logger
    {
    private:
        struct OutboundEntry
        {
            SocketPtr iSocket;
            uint64_t iTraffic;

            bool operator<( const OutboundEntry& rhs ) const
            {
                return iSocket < rhs.iSocket;
            }

            OutboundEntry( SocketPtr aSocket ) 
                : iSocket( aSocket ), iTraffic( 0 ) {}
        };

        typedef std::set<OutboundEntry> OutboundList;
        
        struct RouteInfoEntry
        {
            SocketPtr iInbound;
            uint64_t iInboundTraffic;
            OutboundList iOutboundList;

            RouteInfoEntry() : iInboundTraffic( 0 ) {}
        };

        typedef std::vector<RouteInfoEntry> RouteInfo;

    public:
        Repeater();

        ~Repeater();

        void run
            (
            bool aNonblocking = true
            );

        void stop();

        bool addRoute
            (
            SocketPtr aInbound,
            SocketPtr aOutbound
            );

        bool addRoute
            (
            const RouteInfoEntry& aRouteInfoEntry
            );

        bool addLocalRoute
            (
            short aFromPort,
            short aToPort
            );

        void removeLocalRoute
            (
            short aFromPort,
            short aToPort
            );

        std::string getLocalRouteInfo() const;

        uint64_t getLocalRouteInboudTraffic
            (
            short aInboundPort
            ) const;

        int getLocalRouteOutqBytes
            (
            short aOutboundPort
            );

    private:
        void doRepeat();

        void updateMaxSocketFd
            (
            int aNewFd
            );

        void dumpLocalRouteInfo();

    private:
        fd_set iFdConnection;

        RouteInfo iRouteInfo;

        boost::thread* iRunningThread;

        int iMaxSocketFd;

        bool iForceStop;

        mutable boost::mutex iMutex;

        boost::condition_variable iCond;
    };
}
#endif