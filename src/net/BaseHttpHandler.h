#ifndef NET_BASE_HTTP_SERVER_H
#define NET_BASE_HTTP_SERVER_H

#include "TcpSocket.h"
#include <sys/file.h>

namespace net
{
    // Basically a functor
    class BaseHttpHandler
    {
    public:
        enum MimeType
        {
            MINE_TYPE_NONE,
            MINE_TYPE_MP2T,
            MINE_TYPE_M3U8,
            MINE_TYPE_HTML,
            MINE_TYPE_JPG,
            MINE_TYPE_GIF,
            MINE_TYPE_PNG,
            MINE_TYPE_CSS,
            MINE_TYPE_MP3,
            MINE_TYPE_WAV,
        };

        struct HttpRequest
        {
            std::string iCommand;
            std::string iRequestString;
            std::string iHttpVer;
            std::string iUserAgent;
            bool        iIsKeepAlive;

            static HttpRequest* create
                (
                const std::vector<std::string>& aHeaderLines
                );
        };

    public:
        BaseHttpHandler();

        virtual ~BaseHttpHandler();

        void operator()
            (
            TcpSocketPtr aClientSocket
            );

    //
    // Overridable
    //
    protected:
        virtual void handleRequest
            (
            const HttpRequest& aHttpRequest
            ) = 0;

        virtual int socketSend
            (
            const char* aBuffer,
            size_t aBufferSize
            );

    //
    // Non-overridable
    //
    protected:
        void responseError
            (
            int aRetCode,
            const std::string& aTitle,
            const std::string& aMessage
            );

        bool sendFile
            (
            const HttpRequest& aHttpRequest,
            const MimeType aMineType,
            const std::string& aFileName
            );

    //
    // Static functions
    //
    public:    
        static MimeType getMineType
            (
            const std::string& aFileName
            );

    //
    // Attributes
    //
    protected:
        TcpSocketPtr iClientSocket;
    };
}

#endif

