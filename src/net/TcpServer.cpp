#include "TcpServer.h"

namespace net
{
    /**
     * A general TCP listening server
     * @param aListeningPort
     * @param aMaxThreadNum
     */
    TcpServer::TcpServer
        (
        short aListeningPort,
        int aMaxThreadNum
        )
        : utl::Logger( "TcpServer" )
        , iListeningPort( aListeningPort )
        , iThreadPool( 0, aMaxThreadNum + 1 ) // one for listening
        , iTerminated( true )
    {
    }

    /**
     * Dtor
     */
    TcpServer::~TcpServer()
    {
    }

    /**
     * Start running the server
     * @param aBlocking
     */
    void TcpServer::run
        (
        bool aBlocking
        )
    {
        iTerminated = false;
        iListeningSocket = TcpSocket::Create();
        iListeningSocket->listen( iListeningPort );

        if( aBlocking )
        {
            listenLoop();
        }
        else
        {
            iThreadPool.postTask
                (
                boost::bind( &TcpServer::listenLoop, this ),
                "listenLoop"
                );
        }
    }

    /**
     * The main loop to listen
     */
    void TcpServer::listenLoop()
    {
        while( !iTerminated )
        {
            trace( "Accepting..." );
            net::SocketPtr clientSocket = iListeningSocket->accept();
            if( !clientSocket || !clientSocket->isValid() )
            {
                error( "Accepting failed!" );
                break;
            }

            trace( "Client request is accepted!" );
            
            if( !iTerminated )
            {
                trace( "Post onClientAccepted" );
                iThreadPool.postTask
                    (
                    boost::bind( &TcpServer::onClientAccepted, this, Socket::castTcp( clientSocket ) ),
                    "onClientAccepted"
                    );
            }
        }
    }

    /**
     * Stop listening
     */
    void TcpServer::stop()
    {
        iTerminated= true;
        iListeningSocket->close();
        iThreadPool.joinAll();
        iListeningSocket.reset();
    }

}

