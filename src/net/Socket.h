#ifndef NET_SOCKET_H
#define NET_SOCKET_H

#include <string>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <boost/shared_ptr.hpp>
#include <boost/pointer_cast.hpp>
#include "utl/Logger.h"

namespace net
{
    enum { TIMEOUT_INFINITE = -1 };
    
    class Socket;
    class TcpSocket;
    class UdpSocket;
    
    typedef boost::shared_ptr<Socket>    SocketPtr;
    typedef boost::shared_ptr<TcpSocket> TcpSocketPtr;
    typedef boost::shared_ptr<UdpSocket> UdpSocketPtr;

    class Socket : private utl::Logger
    {    
    public:
        enum SocketType
        {
            SOCKET_TYPE_TCP,
            SOCKET_TYPE_UDP,
            SOCKET_TYPE_UNKNOWN,
        };

    protected:
        class AutoSetNonblock
        {
        public:
            AutoSetNonblock( Socket* s ) : iSocket( s ) 
            {
                iSocket->setNonblock();
            }
            
            ~AutoSetNonblock() { iSocket->unsetNonblock(); }

        private:
            Socket* iSocket;
        };
        
    public:
        Socket
            (
            SocketType aSocketType
            );
            
        virtual ~Socket();

        //
        // MUST-Implement interfaces
        //
    protected:
        virtual int posixReceive
            (
            char* aBuffer,
            int aRcvSize,
            int aTimeout = TIMEOUT_INFINITE
            ) = 0;
    
        virtual int posixPeek
            (
            char* aBuffer,
            int aRcvSize,
            int aTimeout = TIMEOUT_INFINITE
            ) = 0;

        virtual int posixSend
            (
            const char* aBuffer,
            int aRcvSize,
            int aTimeout = TIMEOUT_INFINITE
            ) = 0;

    public:        
        //
        // Non-overridable
        //
        int receive
            (
            char* aBuffer,
            int aRcvSize,
            int aTimeout = TIMEOUT_INFINITE
            );

        int peek
            (
            char* aBuffer,
            int aRcvSize,
            int aTimeout = TIMEOUT_INFINITE
            );

        bool receiveLine
            (
            std::string& aLine,
            int aTimeout = TIMEOUT_INFINITE
            );
        
        int send
            (
            const char* aBuffer,
            int aSendSize,
            int aTimeout = TIMEOUT_INFINITE
            );

        short getBoundPort() const { return iBoundPort; }

        bool bind
            (
            short aPort,
            const std::string& aAcceptedAddress
            );
        
        bool isValid() { return ( -1 != iSocket ); }

        int close();

        bool operator<( const Socket& aAnother ) const
        {
            return iSocket < aAnother.iSocket;
        }

        bool operator==( const Socket& aAnother ) const
        {
            return iSocket == aAnother.iSocket;
        }

        void setNonblock();

        void unsetNonblock();

        //
        // use for select
        //
        void setFd( fd_set& aFdSet ) const { FD_SET( iSocket, &aFdSet ); }

        void clearFd( fd_set& aFdSet ) const { FD_CLR( iSocket, &aFdSet ); }

        bool isFdSet( const fd_set& aFdSet ) const { return FD_ISSET( iSocket, &aFdSet ); }

        // this is bad but we need the socket fd info for some adaptive usage
        int getSocketFd() const { return iSocket; }
        
        SocketType getType() const { return iType; }

        // 
        // Casting
        //
        static TcpSocketPtr castTcp
            (
            SocketPtr aSocket
            );
        
        static UdpSocketPtr castUdp
            (
            SocketPtr aSocket
            );
    
    protected:
        sockaddr_in createSockAddrIn
            (
            const std::string& aIpAddress,
            short aPort
            );

        int select
            (
            int aTimeoutMs
            );


    private:
        void shiftInternalBuffer
            (
            int aShiftedAmount
            );
        
        bool extractLineFromInternalBuffer
            (
            std::string& aLine
            );
        
    protected:
        int iSocket;

        short iBoundPort;

        bool iIsBound;

        sockaddr_in iBoundSockAddr;

        SocketType iType;

        bool iClosed;

    private:
        std::vector<char> iInternalBuffer;

        char iLastLineEndChar;
    };
}

#endif

