#ifndef NET_MISC_H
#define NET_MISC_H

#include <string>

namespace net
{
    enum Protocol
    {
        PROTOCOL_TCP,
        PROTOCOL_UDP,
        PROTOCOL_HTTP,
        PROTOCOL_RTP,
        PROTOCOL_RTSP,
        PROTOCOL_UNKNOWN,
    };

    std::string getLocalIp();

    std::string encodeUrl
        (
        const std::string& aUrl
        );

    std::string decodeUrl
        (
        const std::string& aUrl
        );

    Protocol getProtocol
        (
        const std::string& aUrl
        );
    
}

#endif

