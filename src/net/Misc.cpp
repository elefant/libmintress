#include "Misc.h"
#include "utl/String.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h> 
#include <string.h> 
#include <arpa/inet.h>
#include <stdlib.h>
#include <boost/algorithm/string.hpp>

namespace 
{
    std::string encodeUri
        (
        const std::string& aUri
        );

    bool decodeUriOnce
        (
        const std::string& aUrl,
        std::string& aDecodedUrl
        );
}

namespace net
{
    /**
     * Get remote IP
     * @return 
     */
    std::string getLocalIp()
    {
        struct ifaddrs* ifAddrStruct = NULL;
        getifaddrs( &ifAddrStruct );
        std::string ip;
        for( struct ifaddrs* ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next ) 
        {
            if( AF_INET == ifa->ifa_addr->sa_family &&
                0 == strncmp( ifa->ifa_name, "eth0", 4 ) )
            {   
                void* tmpAddrPtr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
                char addressBuffer[INET_ADDRSTRLEN + 1] = { 0 };
                inet_ntop( AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN );
                ip = addressBuffer;
                break;
            }
        }
        
        if( NULL != ifAddrStruct ) 
        {
            freeifaddrs( ifAddrStruct );
        }

        return ip;
    }

    /**
     * Encode URL
     * @param aUrl
     * @return 
     */
    std::string encodeUrl
        (
        const std::string& aUrl
        )
    {
        if( std::string::npos != aUrl.find( '%' ) )
        {
            return aUrl; // the URL must be encoded. Directly return
        }
        
        return encodeUri( aUrl );
    }

    /**
     * Encode URL
     * @param aUrl
     * @return 
     */
    std::string decodeUrl
        (
        const std::string& aUrl
        )
    {
        std::string decoded = aUrl;
        while( decodeUriOnce( std::string( decoded ), decoded ) );
        return decoded;
    }

    Protocol getProtocol
        (
        const std::string& aUrl
        )
    {
        std::string url = aUrl;
        boost::algorithm::to_lower( url );

        #define ESCAPED "%3a%2f%2f" // an escaped version of "://"  

        const struct 
        {
            Protocol iProtocol;
            std::string iKeyword;
        } PROTOCOL_KEYWORD_TABLE[] =
        {
            { PROTOCOL_TCP,  "tcp://" },
            { PROTOCOL_TCP,  "tcp"ESCAPED },
            { PROTOCOL_UDP,  "udp://" },
            { PROTOCOL_UDP,  "udp"ESCAPED },
            { PROTOCOL_HTTP, "http://" },
            { PROTOCOL_HTTP, "http"ESCAPED },
            { PROTOCOL_RTP,  "rtp://" },
            { PROTOCOL_RTP,  "rtp"ESCAPED },
            { PROTOCOL_RTSP, "rtsp://" },
            { PROTOCOL_RTSP, "rtsp"ESCAPED },
        };

        for( size_t i = 0; i < sizeof( PROTOCOL_KEYWORD_TABLE ) / sizeof( PROTOCOL_KEYWORD_TABLE[0] ); i++ )
        {
            if( std::string::npos != url.find( PROTOCOL_KEYWORD_TABLE[i].iKeyword ) )
            {
                return PROTOCOL_KEYWORD_TABLE[i].iProtocol;
            }        
        }

        return PROTOCOL_UNKNOWN;
    }
}

namespace 
{
    std::string encodeUri
        (
        const std::string& aUri
        )
    {
        std::string encoded;
        for( size_t i = 0; i < aUri.length(); i++ )
        {
            char c = aUri[i];
            if( isalnum( c ) || std::string::npos != std::string( "./@:-_:;&=~!()*'?" ).find( c ) )
            {
                encoded += c;
            }
            else
            {
                encoded += utl::strFormat( "%%%02X", c );
            }
        }

        return encoded;
    }

    bool decodeUriOnce
        (
        const std::string& aUri,
        std::string& aDecodedUri
        )
    {
        bool everDecoded = false;
        aDecodedUri = "";
        for( size_t i = 0; i < aUri.length(); i++ )
        {
            char c = aUri[i];
            if( '%' == c )
            {
                if( i + 2 < aUri.length() &&
                    isxdigit( aUri[i + 1] ) && isxdigit( aUri[i + 2] ) )
                {
                    char hexBuf[] = { aUri[i + 1], aUri[i + 2], '\0' };
                    aDecodedUri += utl::strFormat( "%c", strtol( hexBuf, NULL, 16) );
                    i += 2;
                    everDecoded = true;
                }
                else
                {
                    aDecodedUri = aUri;
                    return false;
                }
            }
            else
            {
                aDecodedUri += c;
            }
        }

        return everDecoded;
    }
}

