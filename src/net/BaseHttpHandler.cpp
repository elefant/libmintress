#include "BaseHttpHandler.h"
#include <vector>
#include <sstream>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>

#include "dshlsbw.h"

#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
#define DS_WEB_SERVER "Disternet Server/1.0"

namespace
{
    const struct
    {
        char iExt[6];
        net::BaseHttpHandler::MimeType iType;
        std::string iMineTypeString;
    } HTML_MINE_TYPE_DATA_LIST[] =
    {
        {".ts",   net::BaseHttpHandler::MINE_TYPE_MP2T, "video/MP2T"},
        {".m3u8", net::BaseHttpHandler::MINE_TYPE_M3U8, "application/x-mpegURL"},
        {".html", net::BaseHttpHandler::MINE_TYPE_HTML, "text/html; charset=iso-8859-1"},
        {".htm",  net::BaseHttpHandler::MINE_TYPE_HTML, "text/html; charset=iso-8859-1"},
        {".jpg",  net::BaseHttpHandler::MINE_TYPE_JPG,  "image/jpeg"},
        {".jpeg", net::BaseHttpHandler::MINE_TYPE_JPG,  "image/jpeg"},
        {".gif",  net::BaseHttpHandler::MINE_TYPE_GIF,  "image/gif"},
        {".png",  net::BaseHttpHandler::MINE_TYPE_PNG,  "image/png"},
        {".css",  net::BaseHttpHandler::MINE_TYPE_CSS,  "text/css"},
        {".mp3",  net::BaseHttpHandler::MINE_TYPE_MP3,  "audio/mpeg"},
        {".wav",  net::BaseHttpHandler::MINE_TYPE_WAV,  "audio/wav"}
    };

    const int HTML_MIME_TYPE_SIZE
        = sizeof( HTML_MINE_TYPE_DATA_LIST ) / sizeof( HTML_MINE_TYPE_DATA_LIST[0] );

    std::string getResponseHeader
        (
        int aRetCode,
        const std::string& aTitle,
        const std::string& aMineType,
        size_t aLengh,
        time_t aLastModified
        );
}

namespace net
{
    /**
     * Ctor
     */
    BaseHttpHandler::BaseHttpHandler()
    {
    }

    /**
     * Dtor
     */
    BaseHttpHandler::~BaseHttpHandler()
    {
    }

    /**
     * The main interface. If the request is successfully parsed, delegate
     * the request to handleRequest() which must be implemented by derived
     * class
     * @param aClientSocket
     */
    void BaseHttpHandler::operator()
        (
        TcpSocketPtr aClientSocket
        )
    {
        iClientSocket = aClientSocket;

        const int BUF_SIZE = 2048;
        std::vector<char> buf( BUF_SIZE, 0 );

        std::vector<std::string> headerLines;
        std::string line;
        while( iClientSocket->receiveLine( line ) && !line.empty() )
        {
            headerLines.push_back( line );
        }

        if( headerLines.empty() )
        {
            return responseError( 888, "Receving request timeout", "Receving request timeout" );
        }

        HttpRequest* request = HttpRequest::create( headerLines );
        if( NULL == request )
        {
            return responseError( 888, "Request parsing error", "Request parsing error" );
        }

        handleRequest( *request );
        delete request;
    }

    /**
     * Response a error page
     * @param aRetCode
     * @param aTitle
     * @param aMessage
     */
    void BaseHttpHandler::responseError
        (
        int aRetCode,
        const std::string& aTitle,
        const std::string& aMessage
        )
    {
        std::stringstream ss;
        std::string htmlBody;
        ss << "<html><head><title>" << aRetCode << " " << aTitle;
        ss << "</title></head>\n<body bgcolor=\"#cc3333\"><h1>" << aRetCode << " " << aTitle << "</h1>\n";
        ss << aMessage << "\n";
        htmlBody = ss.str();
        std::string errMsg = getResponseHeader( aRetCode, aTitle, "text/html", htmlBody.length(), 0) + htmlBody;
        iClientSocket->send( errMsg.c_str(), errMsg.length(), 0 ) ;

        iClientSocket->close();
    }

    /**
     * Transfer a file to the client
     * @param aHttpRequest
     * @param
     * @param aFileName
     * @param aSb
     */
    bool BaseHttpHandler::sendFile
        (
        const HttpRequest& aHttpRequest,
        const MimeType, // aMineType,
        const std::string& aFileName
        )
    {
        int fd = open( aFileName.c_str(), O_RDONLY );
        if( -1 == fd )
        {
            responseError( 400, "Forbidden", "Cannot open file" );
            return false;
        }
        
        int fileSize = boost::filesystem::file_size( aFileName );
        std::string headerText = boost::str
            (
            boost::format
                (
                "%s 200 OK\n"
                "Content-Length: %d\n"
                "Connection: close\n"
                "\n"
                )
            % aHttpRequest.iHttpVer
            % fileSize
            );

        std::vector<char> buffer( headerText.size() + fileSize );
        if( read( fd, &buffer[headerText.size()], fileSize ) != fileSize )
        {
            close( fd );
            responseError( 400, "Read error ","Cannot read file" );
            return false;
        }
        close( fd );

        strncpy( &buffer[0], headerText.c_str(), headerText.size() );

        iClientSocket->setNoDelay();
        int rc = socketSend( &buffer[0], buffer.size() );

        return true;
    }

    /**
     * The core sending function used by sendFile.
     * @param aBuffer
     * @param aBufferSize
     * @return 
     */
    int BaseHttpHandler::socketSend
        (
        const char* aBuffer,
        size_t aBufferSize
        )
    {
        return iClientSocket->send
            (
            aBuffer,
            aBufferSize
            ); 
    }

    /**
     * Get MIME type
     * @return
     */
    BaseHttpHandler::MimeType BaseHttpHandler::getMineType
        (
        const std::string& aFileName
        )
    {
        net::BaseHttpHandler::MimeType retType = net::BaseHttpHandler::MINE_TYPE_NONE;
        std::string::size_type pos = aFileName.find_last_of('.');
        if(pos != std::string::npos)
        {
            std::string fileExt = aFileName.substr( pos );
            std::transform( fileExt.begin(), fileExt.end(), fileExt.begin(), ::tolower );
            for(int i=0; i< HTML_MIME_TYPE_SIZE; i++)
            {
                if (fileExt == HTML_MINE_TYPE_DATA_LIST[i].iExt)
                {
                    retType = HTML_MINE_TYPE_DATA_LIST[i].iType;
                    break;
                }
            }
        }
        return retType;
    }

    /**
     * Create the http request header
     * @return
     */
    BaseHttpHandler::HttpRequest* BaseHttpHandler::HttpRequest::create
        (
        const std::vector<std::string>& aHeaderLines
        )
    {
        // GET /favicon.ico HTTP/1.1
        // Host: 127.0.0.1:8888
        // User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:10.0) Gecko/20100101 Firefox/10.0
        // Accept: image/png,image/*;q=0.8,*/*;q=0.5
        // Accept-Language: en-us,en;q=0.5
        // Accept-Encoding: gzip, deflate
        // Connection: keep-alive

        if( aHeaderLines.empty() )
        {
            return NULL;
        }

        std::vector<std::string> reqList;
        reqList = boost::split
            (
            reqList,
            aHeaderLines[0],
            boost::is_any_of( " " )
            );

        if( 3 != reqList.size() )
        {
            return NULL;
        }

        // A valid header
        HttpRequest *r = new HttpRequest();

        r->iCommand = reqList[0];
        r->iRequestString = reqList[1];
        r->iHttpVer = reqList[2];

        r->iIsKeepAlive = false; // by default
        std::string cmd;
        std::string value;
        for( size_t i = 1; i < aHeaderLines.size() ; i++ )
        {
            std::string::size_type pos = aHeaderLines[i].find_first_of( ':' );
            if( pos != std::string::npos )
            {
                cmd = aHeaderLines[i].substr( 0, pos - 1 );
                value = aHeaderLines[i].substr( pos + 1 );
                std::transform( cmd.begin(), cmd.end(), cmd.begin(), ::tolower );
                if( "connection" == cmd )
                {
                    if( "keep-alive" == value )
                    {
                        r->iIsKeepAlive = true;
                    }
                }
                else if ( "user-agent" == cmd )
                {
                    r->iUserAgent = value;
                }
            }
        }

        return r;
    }
}

namespace
{

    /**
     * Get response header by given HTTP parameters
     * @param aRetCode
     * @param aTitle
     * @param aMineType
     * @param aLengh
     * @param aLastModified
     * @return
     */
    std::string getResponseHeader
        (
        int aRetCode,
        const std::string& aTitle,
        const std::string& aMineType,
        size_t aLengh,
        time_t aLastModified
        )
    {
        std::stringstream ss;
        time_t now;
        char timebuf[128];
        ss << "HTTP/1.0 " << aRetCode << " " << aTitle << "\r\n";
        ss << "Server: " << DS_WEB_SERVER << "\r\n";
        now = time(NULL);
        strftime( timebuf, sizeof(timebuf), RFC1123FMT, gmtime( &now ) );
        ss << "Date: " << timebuf << "\r\n";
        ss << "Content-Type: " << aMineType << "\r\n";
        if(aLengh > 0)
        {
            ss << "Content-Length: " << aLengh << "\r\n";
        }
        if(aLastModified > 0)
        {
            strftime( timebuf, sizeof(timebuf), RFC1123FMT, gmtime( &aLastModified ) );
            ss << "Last-Modified: " << aLengh << "\r\n";
        }
        ss << "Connection: close\r\n" ;
        ss << "\r\n";
        return ss.str();
    }
}

