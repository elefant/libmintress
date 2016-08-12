#include "Socket.h"
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/thread_time.hpp>

#include "TcpSocket.h"
#include "UdpSocket.h"

namespace
{
    int findLineEnd
        (
        const char* aBuffer,
        int aBufferSize
        );
}

namespace net
{    
    /**
     * ctor
     */
    Socket::Socket
        (
        SocketType aSocketType
        )
        : utl::Logger( "Socket" )
        , iSocket( -1 )
        , iBoundPort( 0 )
        , iIsBound( false )
        , iType( aSocketType )
        , iClosed( false )
        , iLastLineEndChar( '\0' )
    {
    }

    /**
     * Destructor
     */
    Socket::~Socket()
    {
        close();
    }


    /**
     * Close
     */
    int Socket::close()
    {
        int ret = 0;
    
        if( !iClosed )
        {
            ::shutdown( iSocket, SHUT_RDWR );
            ret = ::close( iSocket ); 
            iClosed = true;
        }

        return ret;
    }

    /**
     * For server.
     * @param aAcceptedAddress
     * @param aPort
     * @return 
     */
    bool Socket::bind
        (
        short aPort,
        const std::string& aAcceptedAddress
        )
    {
        iBoundSockAddr = createSockAddrIn( aAcceptedAddress, aPort );
        if( ::bind(iSocket, (struct sockaddr *)&iBoundSockAddr, sizeof(iBoundSockAddr) ) == -1 )
        {
            error( "can not bind port: %d (%s)", aPort, strerror( errno ) );
            return false;
        }

        iIsBound = true;

        if( 0 != aPort )
        {
            iBoundPort = aPort;
        }
        else
        {
            socklen_t len;
            if( getsockname( iSocket, (struct sockaddr *)&iBoundSockAddr, &len ) )
            {
                return false; // something wrong
            }
            iBoundPort = ntohs( iBoundSockAddr.sin_port );
        }
        return true;
    }

    /**
     * Set non-blocking IO
     */
    void Socket::setNonblock()
    {
        int flags = fcntl( iSocket, F_GETFL, 0 );
        fcntl( iSocket, F_SETFL, flags | O_NONBLOCK );
    }

    /**
     * Unset non-blocking IO
     */
    void Socket::unsetNonblock()
    {
        int flags = fcntl( iSocket, F_GETFL, 0 );
        fcntl( iSocket, F_SETFL, flags & ( ~O_NONBLOCK ) );
    }
    
    /**
     * Cast to TcpSocket
     * @param aSocket
     * @return 
     */
    TcpSocketPtr Socket::castTcp
        (
        SocketPtr aSocket
        )
    {
        return TcpSocketPtr( aSocket, boost::detail::dynamic_cast_tag() );
    }

    /**
     * Cast to UdpSocket
     * @param aSocket
     * @return 
     */
    UdpSocketPtr Socket::castUdp
        (
        SocketPtr aSocket
        )
    {
        return UdpSocketPtr( aSocket, boost::detail::dynamic_cast_tag() );
    }
    
    /**
     * A wrapper to create sockaddr_in
     * @param aIpAddress
     * @param aPort
     * @return 
     */
    sockaddr_in Socket::createSockAddrIn
        (
        const std::string& aIpAddress,
        short aPort
        )
    {
        sockaddr_in s;
    
        memset( &s, 0, sizeof(s) );
        s.sin_family = AF_INET;
        s.sin_port = htons( aPort );
        if( aIpAddress.empty() )
        {
            s.sin_addr.s_addr = htonl( INADDR_ANY );
        }
        else
        {
            s.sin_addr.s_addr = inet_addr( aIpAddress.c_str() );
        }

        return s;
    }

    /*
     * Set the timeout
     */
    int Socket::select
        (
        int aTimeoutMs
        )
    {
        fd_set readfds;
        FD_ZERO( &readfds );
        FD_SET( iSocket, &readfds );

        if( aTimeoutMs > 0 )
        {
            struct timeval tv;
            memset( &tv, 0, sizeof( tv ) );
            tv.tv_sec = 0;
            tv.tv_usec = aTimeoutMs * 1000;
            return ::select( iSocket + 1, &readfds, NULL, NULL, &tv );   
        }
        else
        {
            return ::select( iSocket + 1, &readfds, NULL, NULL, NULL );   
        }
    }

    /*
     * Look into the internal buffer, extract the first line and append to the buffer
     */
    bool Socket::extractLineFromInternalBuffer
        (
        std::string& aLine
        )
    {
        if( iInternalBuffer.empty() )
        {
            return false;
        }

        int lineStart = 0;
        if( ( '\r' == iLastLineEndChar && '\n' == iInternalBuffer[0] ) ||
            ( '\n' == iLastLineEndChar && '\r' == iInternalBuffer[0] ) )
        {
            lineStart = 1;
        }

        int lineEnd = findLineEnd
            (
            &iInternalBuffer[lineStart], 
            iInternalBuffer.size() - lineStart 
            );
        
        lineEnd += lineStart;
        
        aLine.append( &iInternalBuffer[lineStart], lineEnd - lineStart );

        bool endOfLineFound = ( (size_t)lineEnd < iInternalBuffer.size() );

        //
        // Update iLastLineEndChar
        //
        
        if( endOfLineFound )
        {
            iLastLineEndChar = iInternalBuffer[lineEnd];
        }
        else
        {
            iLastLineEndChar = '\0';
        }
        
        shiftInternalBuffer( lineEnd + 1 );

        return endOfLineFound;
    }

    /*
     * Shift internal buffer
     */
    void Socket::shiftInternalBuffer
        (
        int aShiftedAmount
        )
    {
        if( (size_t)aShiftedAmount >= iInternalBuffer.size() )
        {
            std::vector<char>().swap( iInternalBuffer );
        }
        else
        {
            size_t newSize = iInternalBuffer.size() - aShiftedAmount;
            memmove
                (
                &iInternalBuffer[0], 
                &iInternalBuffer[aShiftedAmount],
                newSize
                );

            iInternalBuffer.resize( newSize );
        }        
    }
    
    /*
     * Receive line
     */
    bool Socket::receiveLine
        (
        std::string& aLine,
        int aTimeout
        )
    {
        aLine = "";
        
        bool endOfLineFound;

        boost::system_time absTimeout = boost::get_system_time() + 
                                        boost::posix_time::milliseconds( aTimeout );

        enum { READ_LINE_CHUNCK_SIZE = 512 };
        while( !( endOfLineFound = extractLineFromInternalBuffer( aLine ) ) )
        {
            if( TIMEOUT_INFINITE != aTimeout && boost::get_system_time() > absTimeout )
            {
                return false;
            }

            iInternalBuffer.resize( READ_LINE_CHUNCK_SIZE );

            int recvBytes = posixReceive
                (
                &iInternalBuffer[0], 
                iInternalBuffer.size(), 
                100 // use 10ms as the timeout granuality  
                );

            if( recvBytes <= 0 )
            {
                trace( "receiveLine timeout" );
                return false;
            }

            iInternalBuffer.resize( recvBytes );
        }

        return endOfLineFound;
    }

    /*
     * Receive line
     */
    int Socket::receive
        (
        char* aBuffer,
        int aRcvSize,
        int aTimeout
        )
    {
        int copyAmount = std::min( aRcvSize, (int)iInternalBuffer.size() );
        memcpy( aBuffer, &iInternalBuffer[0], copyAmount );

        shiftInternalBuffer( copyAmount );

        int remain = aRcvSize - copyAmount;
        
        if( remain > 0 )
        {
            int recvBytes = posixReceive
                (
                &aBuffer[copyAmount],
                remain,
                aTimeout
                );

            if( recvBytes <= 0 )
            {
                return copyAmount;
            }
            else
            {
                return copyAmount + recvBytes;
            }
        }
        else
        {
            return aRcvSize;
        }
    }

    /*
     * Receive line
     */
    int Socket::peek
        (
        char* aBuffer,
        int aRcvSize,
        int aTimeout
        )
    {
        int copyAmount = std::min( aRcvSize, (int)iInternalBuffer.size() );
        memcpy( aBuffer, &iInternalBuffer[0], copyAmount );

        int remain = aRcvSize - copyAmount;
        
        if( remain > 0 )
        {
            int recvBytes = posixPeek
                (
                &aBuffer[copyAmount],
                remain,
                aTimeout
                );

            if( recvBytes <= 0 )
            {
                return copyAmount;
            }
            else
            {
                return copyAmount + recvBytes;
            }
        }
        else
        {
            return aRcvSize;
        }
    }

    /**
     * Send to remote if connected
     * @param aBuffer
     * @param aSendSize
     * @param aTimeout
     * @return 
     */
    int Socket::send
        (
        const char* aBuffer,
        int aSendSize,
        int
        )
    {
        int remain = aSendSize;
        int totalSent = 0;
        while( 0 != remain )
        {
            int sendSz = posixSend( &aBuffer[aSendSize - remain], remain );
            if( sendSz <= 0 )
            {
                return totalSent + sendSz;
            }

            totalSent += sendSz;
            remain -= sendSz;
        }
        
        return aSendSize;
    }
}

namespace
{
    /*
     * Find the end of the line in buffer.
     * Returns the position of the end of the line or aBufferSize if no EOL found
     */
    int findLineEnd
        (
        const char* aBuffer,
        int aBufferSize
        )
    {
        for( int i = 0; i < aBufferSize; i++ )
        {
            if( '\r' == aBuffer[i] || '\n' == aBuffer[i] )
            {
                return i;
            }
        }
        
        return aBufferSize;
    }
}
