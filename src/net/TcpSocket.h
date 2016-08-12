#ifndef NET_TCP_SOCKET_H
#define NET_TCP_SOCKET_H

#include "Socket.h"

namespace net
{
    class TcpSocket : public Socket
    {
    public:
        static TcpSocketPtr Create();

        //
        // Socket common interface
        //
    private:
        virtual int posixReceive
            (
            char* aBuffer,
            int aRecvSize,
            int aTimeout = TIMEOUT_INFINITE
            );
        
        virtual int posixPeek
            (
            char* aBuffer,
            int aRcvSize,
            int aTimeout = TIMEOUT_INFINITE
            );

        virtual int posixSend
            (
            const char* aBuffer,
            int aSendSize,
            int aTimeout = TIMEOUT_INFINITE
            );

    public:
        //
        // TCP interface
        //
        bool connect
            (
            const std::string &aRemoteIpAddress,
            short aRemotePort
            );
        
        bool listen
            (
            short aListeningPort,
            const std::string& aAcceptIp = std::string()
            );

        SocketPtr accept();

        void setNoDelay();
         
    private:
        TcpSocket() ;
        
        TcpSocket( int aSocket );

    private:

    };
}

#endif
