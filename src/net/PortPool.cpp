#include "net/PortPool.h"
#include "utl/String.h"
#include "net/TcpSocket.h"
#include "net/UdpSocket.h"

namespace net
{
    /**
     * Ctor
     * @param aProtocol
     * @param aRangeStart
     * @param aRangeEnd
     */
    PortPool::PortPool
        (
        Protocol aProtocol,
        short aRangeStart,
        short aRangeEnd
        )
        : utl::Logger( utl::strFormat( "PortPool(%d, %d)", aRangeStart, aRangeEnd ) )
        , iProtocol( aProtocol )
    {
        for( short i = aRangeStart; i <= aRangeEnd; i++ )
        {
            add( i );
        }
    }

    /**
     * Dtor
     */
    PortPool::~PortPool()
    {
    }

    /**
     * Acquire a port
     * @param aResource
     * @return 
     */
    bool PortPool::acquire
        (
        short& aResource
        )
    {
        short destPort;
        std::vector<short> pendingPortList;
        while( 1 )
        {
            if( !utl::ResourcePool<short>::acquire( destPort ) )
            {
                error( "No available port" );
                return false;
            }          

            net::SocketPtr tempSocket;
            if( PROTOCOL_TCP == iProtocol )
            {
                tempSocket = net::TcpSocket::Create();
            }
            else
            {
                tempSocket = net::UdpSocket::Create();
            }
            
            if( !tempSocket->bind( destPort, "" ) )
            {
                pendingPortList.push_back( destPort );
                trace( "%d might be not closed temporarily", destPort );
            }
            else
            {
                trace( "We got the available port!: %d", destPort );
                aResource = destPort;
                break;
            }
        }

        // release pending ports
        for( size_t i = 0; i < pendingPortList.size(); i++ )
        {
            release( pendingPortList[i] );
        }
        
        return true;
    }
}

