#include "TcpSocket.h"
#include <netinet/tcp.h>
#include <fcntl.h>

namespace net
{
    /**
     * Constructor
     */
    TcpSocket::TcpSocket()
        : Socket( SOCKET_TYPE_TCP )
    {
        iSocket = socket( AF_INET, SOCK_STREAM, 0 );
        int optval = 1;
        setsockopt( iSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof( optval ) );
    }

    /**
     * Constructor
     */
    TcpSocket::TcpSocket
        (
        int aSocket
        )
        : Socket( SOCKET_TYPE_TCP )
    { 
        iSocket = aSocket;
    }

    /**
     * Create TCP socket pointer
     * @return 
     */
    TcpSocketPtr TcpSocket::Create()
    {
        return TcpSocketPtr( new TcpSocket() );
    }
    
    /**
     * Connect to some remote server
     * @param aRemotePort
     * @param aRemoteIpAddress
     * @return 
     */
    bool TcpSocket::connect
        (
        const std::string& aRemoteIpAddress,
        short aRemotePort
        )
    {
        sockaddr_in servaddr = createSockAddrIn( aRemoteIpAddress, aRemotePort );

        if ( ::connect( iSocket, (struct sockaddr *) &servaddr, sizeof(servaddr) ) < 0 )
        {   
            return false;
        }

        return true;
    }

    /**
     * Send to remote if connected
     * @param aBuffer
     * @param aSendSize
     * @param aTimeout
     * @return 
     */
    int TcpSocket::posixSend
        (
        const char* aBuffer,
        int aSendSize,
        int
        )
    {
        return ::send( iSocket, aBuffer, aSendSize, MSG_NOSIGNAL );
    }

    /**
     * Receive from remote
     * @param aBuffer
     * @param aRecvSize
     * @param aTimeout
     * @return 
     */
    int TcpSocket::posixReceive
        (
        char* aBuffer,
        int aRecvSize,
        int aTimeout
        )
    {
        AutoSetNonblock setNonblock( this );

        if( select( aTimeout ) > 0 )
        {
            return recv( iSocket, aBuffer, aRecvSize, 0 );
        }

        return -1; // timeout        
    }

     /**
     * Peek from remote
     * @param aBuffer
     * @param aRecvSize
     * @param aTimeout
     * @return 
     */
    int TcpSocket::posixPeek
        (
        char* aBuffer,
        int aRecvSize,
        int aTimeout
        )
    {
        AutoSetNonblock setNonblock( this );
    
        if( select( aTimeout ) > 0 )
        {
            return recv( iSocket, aBuffer, aRecvSize, MSG_PEEK );
        }

        return -1;
    }
    
    /**
     * Listen to the given port
     * @param aListeningPort
     * @return 
     */
    bool TcpSocket::listen
        (
        short aListeningPort,
        const std::string& aAcceptIp
        )
    {
        if( !bind( aListeningPort, aAcceptIp ) )
        {
            return false;
        }
        
        return ( 0 == ::listen( iSocket, 5 ) );
    }

    /**
     * Accept connection from remote
     * @return 
     */
    SocketPtr TcpSocket::accept()
    {
        int sd = ::accept( iSocket, NULL, NULL );
        if( sd < 0 )
        {
            return SocketPtr();
        }
        
        return SocketPtr( new TcpSocket( sd ) );
    }

    /**
     * Set no delay
     * @return 
     */
    void TcpSocket::setNoDelay()
    {
        int flag = 1;
        setsockopt( iSocket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag) );
    }
}

