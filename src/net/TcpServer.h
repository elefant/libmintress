#ifndef NET_TCP_SERVER_H
#define NET_TCP_SERVER_H

#include "TcpSocket.h"
#include "sys/ThreadPool.h"
#include "utl/Logger.h"

namespace net
{
    class TcpServer : private utl::Logger
    {
    public:
        TcpServer
            (
            short aListeningPort,
            int aMaxThreadNum
            );

        virtual ~TcpServer();

        virtual void onClientAccepted
            (
            TcpSocketPtr aClientSocket
            ) = 0;

        void run
            (
            bool aBlocking = true
            );

        void stop();

    private:
        void listenLoop();

    protected:
        TcpSocketPtr iListeningSocket;

        short iListeningPort;

        sys::ThreadPool iThreadPool;

        bool iTerminated;
    };
}

#endif

