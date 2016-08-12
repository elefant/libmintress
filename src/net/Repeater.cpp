#include "Repeater.h"
#include <boost/thread/thread.hpp>
#include <string.h>
#include "utl/String.h"

namespace net
{
    /**
    * Constructor
    */
    Repeater::Repeater()
        : utl::Logger( "Repeater" )
        , iRunningThread( NULL )
        , iMaxSocketFd( 0 )
        , iForceStop( false )
    {
        FD_ZERO( &iFdConnection );
    }

    /**
    * Destructor
    */
    Repeater::~Repeater()
    {
        if( iRunningThread )
        {   
            stop();
        }
        delete iRunningThread;
    }

    /**
    * Start the working thread
    */
    void Repeater::run
        (
        bool aNonblocking
        )
    {
        if( aNonblocking )
        {
            iRunningThread = new boost::thread
                (
                boost::bind( &Repeater::doRepeat, this )
                );
        }
        else
        {
            doRepeat();
        }
    }

    /**
    * Stop repeating
    */
    void Repeater::stop()
    {
        if( iRunningThread )
        {
            iCond.notify_one();
        
            iForceStop = true;
            iRunningThread->join();

            delete iRunningThread;
            iRunningThread = NULL;
            iForceStop = false;
            iRouteInfo.clear();
        }
    }

    /**
    * Stop repeating
    */
     bool Repeater::addRoute
        (
        SocketPtr aInbound,
        SocketPtr aOutbound
        )
    {
        RouteInfoEntry re;
        re.iInbound = aInbound;
        re.iOutboundList.insert( aOutbound );
        return addRoute( re );
    }

    /**
    * Stop repeating
    */
    bool Repeater::addRoute
        (
        const RouteInfoEntry& aRouteInfoEntry
        )
    {
        boost::mutex::scoped_lock lock( iMutex );
    
        iCond.notify_one();
        
        // Update iFdConnection accordingly
        aRouteInfoEntry.iInbound->setFd( iFdConnection );
        updateMaxSocketFd( aRouteInfoEntry.iInbound->getSocketFd() );
        
        // Find if there any existing inbound socket
        RouteInfoEntry *re = NULL;
        for( size_t i = 0; i < iRouteInfo.size(); i++ )
        {
            if( (*iRouteInfo[i].iInbound) ==  (*aRouteInfoEntry.iInbound) )
            {
                re = &iRouteInfo[i];
                break;
            }
        }

        if( NULL == re ) // new inbound socket added
        {
            iRouteInfo.push_back( aRouteInfoEntry );
            return true;
        }

        // the inbound socket is existed in our route info, append outbound list
        OutboundList::iterator i = aRouteInfoEntry.iOutboundList.begin();
        for( ; i != aRouteInfoEntry.iOutboundList.end(); ++i )
        {
            re->iOutboundList.insert( *i );
        }

        return true;
    }
    
    /**
    * Stop repeating
    */
    void Repeater::updateMaxSocketFd
        (
        int aNewFd
        )
    {
        iMaxSocketFd = std::max( iMaxSocketFd, aNewFd );
    }

    /**
    * Stop repeating
    */
    bool Repeater::addLocalRoute
        (
        short aFromPort,
        short aToPort
        )
    {
        boost::mutex::scoped_lock lock( iMutex );
    
        // Search existing inbound socket first 
        SocketPtr from;
        OutboundList* outboundList = NULL;
        for( size_t i = 0; i < iRouteInfo.size(); i++ )
        {
            if( Socket::SOCKET_TYPE_UDP == iRouteInfo[i].iInbound->getType() &&
                iRouteInfo[i].iInbound->getBoundPort() == aFromPort )
            {
                from = iRouteInfo[i].iInbound;
                outboundList = &iRouteInfo[i].iOutboundList;
                break;
            }
        }

        if( from )
        {
            trace( "inbound port is already opened: %d", aFromPort );
        
            // Check if any sending udp port equals to aToPort
            OutboundList::iterator i = outboundList->begin();
            for( ; i != outboundList->end(); ++i )
            {
                UdpSocketPtr outboundUdpSocket = Socket::castUdp( i->iSocket );
                if( outboundUdpSocket && outboundUdpSocket->getSendDestPort() == aToPort )
                {
                    trace( "The in/out pair is already existed" );
                    return true; // no need to addRoute
                }
            }
        }
        else
        {
            trace( "Create new socket for inbound port: %d", aFromPort );
            
            from = UdpSocket::Create(); 
            from->bind( aFromPort, "127.0.0.1" );

            // Remove any out port bound to another in port
            for( size_t i = 0; i < iRouteInfo.size(); i++ )
            {
                OutboundList::iterator os = iRouteInfo[i].iOutboundList.begin();
                for( ; os != iRouteInfo[i].iOutboundList.end(); ++os )
                {
                    UdpSocketPtr outboundUdpSocket = Socket::castUdp( os->iSocket );
                    if( outboundUdpSocket && outboundUdpSocket->getSendDestPort() == aToPort )
                    {
                        iRouteInfo[i].iOutboundList.erase( os );
                        break;
                    }
                }
            }
        }

        lock.unlock();

        // Create a new outbound socket
        UdpSocketPtr to = UdpSocket::Create();
        to->setSendDestination( "127.0.0.1", aToPort );

        if( !addRoute( from, to ) )
        {
            return false;
        }
        
        return true;
    }

    /**
     * Remove local route
     */
    void Repeater::removeLocalRoute
        (
        short aFromPort,
        short aToPort
        )
    {
        boost::mutex::scoped_lock lock( iMutex );

        SocketPtr from;
        OutboundList* outboundList = NULL;
        for( size_t i = 0; i < iRouteInfo.size(); i++ )
        {
            if( Socket::SOCKET_TYPE_UDP == iRouteInfo[i].iInbound->getType() &&
                iRouteInfo[i].iInbound->getBoundPort() == aFromPort )
            {
                from = iRouteInfo[i].iInbound;
                outboundList = &iRouteInfo[i].iOutboundList;
                break;
            }
        }

        if( NULL == from )
        {
            warning( "UDP port: %d is not local routed", aFromPort );
            return;
        }

        // Check if any sending udp port equals to aToPort
        OutboundList::iterator i = outboundList->begin();
        for( ; i != outboundList->end(); ++i )
        {
            UdpSocketPtr outboundUdpSocket = Socket::castUdp( i->iSocket );
            if( outboundUdpSocket && outboundUdpSocket->getSendDestPort() == aToPort )
            {
                outboundList->erase( i );
                trace( "Local route( %d, %d ) is removed successfully", aFromPort, aToPort );
                return;
            }
        }

        error( "UDP port %d is not repeated by port %d", aToPort, aFromPort );
    }

    /**
    * Dump the local route info
    */
    void Repeater::dumpLocalRouteInfo()
    {
        trace( getLocalRouteInfo().c_str() );
    }

    /**
     * Get local route traffic
     */
    uint64_t Repeater::getLocalRouteInboudTraffic
        (
        short aInboundPort
        ) const
    {
        boost::mutex::scoped_lock lock( iMutex );
      
        for( size_t i = 0; i < iRouteInfo.size(); i++ )
        {
            SocketPtr inbound = iRouteInfo[i].iInbound;
            if( Socket::SOCKET_TYPE_UDP == inbound->getType() &&
                inbound->getBoundPort() == aInboundPort )
            {
                return iRouteInfo[i].iInboundTraffic;
            }
        }
        
        return 0;
    }

    /**
     * Get local route output queue bytes
     */
    int Repeater::getLocalRouteOutqBytes
        (
        short aOutboundPort
        )
    {
        boost::mutex::scoped_lock lock( iMutex );
        
        for( size_t i = 0; i < iRouteInfo.size(); i++ )
        {
            if( Socket::SOCKET_TYPE_UDP != iRouteInfo[i].iInbound->getType() )
            {
                continue;
            }

            OutboundList::iterator os = iRouteInfo[i].iOutboundList.begin();
            for( ; os != iRouteInfo[i].iOutboundList.end(); ++os )
            {
                UdpSocketPtr outboundUdpSocket = Socket::castUdp( os->iSocket );
                if( !outboundUdpSocket )
                {
                    continue;
                }
                
                if( outboundUdpSocket->getSendDestPort() == aOutboundPort )
                {
                    return outboundUdpSocket->getOutqBytes();
                }
            }
        }

        return -1;
    }

    /**
     * Get local route traffic
     */
    std::string Repeater::getLocalRouteInfo() const
    {
        boost::mutex::scoped_lock lock( iMutex );
        
        std::string info;
        for( size_t i = 0; i < iRouteInfo.size(); i++ )
        {
            SocketPtr inbound = iRouteInfo[i].iInbound;
            OutboundList::iterator os = iRouteInfo[i].iOutboundList.begin();
            if( Socket::SOCKET_TYPE_UDP == inbound->getType() )
            {
                for( ; os != iRouteInfo[i].iOutboundList.end(); ++os )
                {
                    UdpSocketPtr outboundUdpSocket = Socket::castUdp( os->iSocket );
                    if( outboundUdpSocket )
                    {
                        info += utl::strFormat
                            (
                            "( %d(%llu) ==> %d(%llu) ), ", 
                            inbound->getBoundPort(),
                            iRouteInfo[i].iInboundTraffic,
                            outboundUdpSocket->getSendDestPort(),
                            os->iTraffic
                            );
                    }
                }
            }
        }

        return info.c_str();
    }

    /**
    * Entry point to do the repeating
    */
    void Repeater::doRepeat()
    {
        const int bufSize = 4096;
        std::vector<char> vBuf( bufSize, 0 );
        while( !iForceStop )
        {
            boost::mutex::scoped_lock lock( iMutex );

            if( 0 == iMaxSocketFd )
            {
                iCond.wait( lock );
            }

            lock.unlock();

            fd_set fdAvaiable;
            memcpy( &fdAvaiable, &iFdConnection, sizeof( iFdConnection ) );

            struct timeval timeout;
            timeout.tv_sec  = 0;
            timeout.tv_usec = 500000;

            int rc = select( iMaxSocketFd + 1, &fdAvaiable,  NULL, NULL, &timeout );

            if( rc < 0 ) // select fail
            {
                error( "select failed: %s", strerror( errno ) );
                continue;
            }

            if( rc == 0 ) // Timeout
            {
                continue;
            }

            lock.lock();
            for( size_t i = 0; i < iRouteInfo.size(); i++ )
            {
                if( !iRouteInfo[i].iInbound->isFdSet( fdAvaiable ) )
                {
                    continue;
                }

                // recevie data from one of the inbound socket
                int recvCnt = iRouteInfo[i].iInbound->receive( &vBuf[0], bufSize );
                if( recvCnt <= 0 )
                {
                    continue;
                }

                iRouteInfo[i].iInboundTraffic += recvCnt;

                // We got data from inbound socket. Repeat them to outbound sockets
                OutboundList::iterator si = iRouteInfo[i].iOutboundList.begin();
                for( ; si != iRouteInfo[i].iOutboundList.end(); ++si )
                {
                    //if( (*si)->isFdSet( fdAvaiable ) ) // why? why? why?
                    {
                        if( si->iSocket->send( &vBuf[0], recvCnt ) <= 0 )
                        {   
                            error( "Cannot repeat to socket:%d", si->iSocket->getSocketFd() );
                        }
                        else
                        {
                            // iTraffic is not the criteria to maintain the set property
                            // so it can be changed without worries
                            const_cast<OutboundEntry&>(*si).iTraffic += recvCnt;
                        }
                    }
                }    
            }
        }

        trace( "bye bye~" );
    }

}
