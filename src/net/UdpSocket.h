#ifndef NET_UDP_SOCKET_H
#define NET_UDP_SOCKET_H

#include "Socket.h"

namespace net
{
    class UdpSocket : public Socket
    {
    public:
        static UdpSocketPtr Create();

        //
        // Must implement
        // 
    protected:
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
        // UDP interface
        //
        void setSendDestination
            (
            const std::string& aSendDestIp,
            short aSendDestPort
            );

        short getSendDestPort() const;

        int sendTo
            (
            const char* aBuffer,
            int aSendSize,
            const std::string& aDestIp,
            short aDestPort,
            int aTimeout = TIMEOUT_INFINITE
            );

        int receivFrom
            (
            char* aBuffer,
            int aRecvSize,
            std::string& aFromIp,
            short& aFromPort,
            int aTimeout = TIMEOUT_INFINITE
            );

        int getOutqBytes() const;

    private:    
        UdpSocket();

    private:
        sockaddr_in iSendDestSockAddr;

        bool iIsSendDestSet;
    };
}

#endif
