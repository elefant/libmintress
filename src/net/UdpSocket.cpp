#include "UdpSocket.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/ioctl.h>

namespace net
{
    /**
     * Constructor
     */
    UdpSocket::UdpSocket()
        : Socket( SOCKET_TYPE_UDP )
        , iIsSendDestSet( false )
    {
        iSocket = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
        int optval = 1;
        setsockopt( iSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof( optval ) );
    }

    /**
     * Create UDP socket pointer
     * @return 
     */
    UdpSocketPtr UdpSocket::Create()
    {
        return UdpSocketPtr( new UdpSocket() );
    }

    /**
     * Get dest port
     * @return 
     */
    short UdpSocket::getSendDestPort() const
    {
        if( iIsSendDestSet )
        {
            return ntohs( iSendDestSockAddr.sin_port );
        }
        else
        {
            return 0;
        }
    }

    /**
     * Set the send() destination
     * @param aSendDestIp
     * @param aSendDestPort
     */
    void UdpSocket::setSendDestination
        (
        const std::string& aSendDestIp,
        short aSendDestPort
        )
    {
        iSendDestSockAddr = createSockAddrIn( aSendDestIp, aSendDestPort );
        iIsSendDestSet = true;
    }

    /**
     * Send to the designated destination
     * @param aBuffer
     * @param aSendSize
     * @param aTimeout
     * @return 
     */
    int UdpSocket::posixSend
        (
        const char* aBuffer,
        int aSendSize,
        int
        )
    {
        if( iIsSendDestSet )
        {
            return ::sendto
                (
                iSocket, 
                aBuffer, 
                aSendSize, 
                MSG_NOSIGNAL,                 
                (struct sockaddr*)&iSendDestSockAddr,
                sizeof( iSendDestSockAddr ) 
                );
        }
        else
        {
            return -1;
        }
    }

    /**
     * Receive from anywhere
     * @param aBuffer
     * @param aRecvSize
     * @param aTimeout
     * @return 
     */
    int UdpSocket::posixReceive
        (
        char* aBuffer,
        int aRecvSize,
        int aTimeout
        )
    {    
        AutoSetNonblock setNonblock( this );

        if( select( aTimeout ) > 0 )
        {
            sockaddr_in remote;
            socklen_t len = sizeof( remote );
            return ::recvfrom
                (
                iSocket, 
                aBuffer, 
                aRecvSize, 
                0,
                ( struct sockaddr* )&remote,
                &len
                );
        }

        return -1;
    }
    
     /**
     * Peek from remote
     * @param aBuffer
     * @param aRecvSize
     * @param aTimeout
     * @return 
     */
    int UdpSocket::posixPeek
        (
        char* aBuffer,
        int aRecvSize,
        int aTimeout
        )
    {
        AutoSetNonblock setNonblock( this );

        if( select( aTimeout ) > 0 )
        {
            sockaddr_in remote;
            socklen_t len = sizeof( remote );
            return ::recvfrom
                (
                iSocket, 
                aBuffer, 
                aRecvSize, 
                MSG_PEEK,
                ( struct sockaddr* )&remote,
                &len
                );
        }

        return -1;
    }

    /**
     * Behaves the same as POSIX sendto API
     * @param aBuffer
     * @param aSendSize
     * @param aDestIp
     * @param aDestPort
     * @param aTimeout
     * @return 
     */
    int UdpSocket::sendTo
        (
        const char* aBuffer,
        int aSendSize,
        const std::string& aDestIp,
        short aDestPort,
        int //aTimeout
        )
    {
        sockaddr_in destSockAddr = createSockAddrIn( aDestIp, aDestPort );
        return ::sendto
            (
            iSocket, 
            aBuffer, 
            aSendSize, 
            MSG_NOSIGNAL,                 
            (struct sockaddr*)&destSockAddr,
            sizeof( destSockAddr ) 
            );
    }

    /**
     * Behaves the same as POSIX recefrom API
     * @param aBuffer
     * @param aRecvSize
     * @param aFromIp
     * @param aFromPort
     * @param aTimeout
     * @return 
     */
    int UdpSocket::receivFrom
        (
        char* aBuffer,
        int aRecvSize,
        std::string& aFromIp,
        short& aFromPort,
        int aTimeout
        )
    {
        if( select( aTimeout ) > 0 )
        {
            sockaddr_in remote;
            socklen_t len = sizeof( remote );
            int ret = ::recvfrom
                (
                iSocket, 
                aBuffer, 
                aRecvSize, 
                0,
                ( struct sockaddr* )&remote,
                &len
                );

            aFromIp = std::string( inet_ntoa( remote.sin_addr ) );
            aFromPort = ntohs( remote.sin_port );

            return ret;
        }

        return -1;
    }

    /**
     * Get udp output q bytes
     * @return 
     */
    int UdpSocket::getOutqBytes() const
    {
        int pbytes = 0;
        if( 0 == ioctl( iSocket, TIOCOUTQ, &pbytes ) )
        {
            return pbytes;
        }

        return -1;
    }
    
}

