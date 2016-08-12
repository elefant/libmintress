#ifndef NET_CURL_SESSION_H
#define NET_CURL_SESSION_H

#include <boost/function.hpp>
#include <curl/curl.h>

namespace net
{
    class CurlSession
    {
    public:
        typedef boost::function<size_t( void*, size_t, size_t )> WriteCallback;
        
        typedef boost::function<int( double, double, double, double )> OnProgressCallback;
    
        enum{ INVALID_HTTP_RESPONSE_CODE = -1 };
        
    public:
        CurlSession();

        ~CurlSession();

        CURLcode performGetContentLength
            (
            const std::string& aUrl,
            double& aContentLength,
            int aTimeoutMs = 15 * 1000 // default 15 seconds
            );

        CURLcode performGetHeader
            (
            const std::string& aUrl
            );

        CURLcode performDownload
            (
            const std::string& aUrl,
            long& aResponseCode,
            const std::string& aRange = "0-",
            const WriteCallback& aWriteCallback = WriteCallback(),
            const OnProgressCallback& aOnProgressCallback = OnProgressCallback()
            );

        CURLcode getInfo
            (
            CURLINFO aWhatInfo,
            void* aInfo
            );

    private:
        static size_t writeCallback
            (
            void *ptr, 
            size_t size, 
            size_t nmemb, 
            CurlSession* aThis
            );

        static int onProgressCallback
            (
            CurlSession* aThis,
            double dltotal,
            double dlnow,
            double ultotal,
            double ulnow
            );

        static size_t writeHeader
            (
            void *ptr, 
            size_t size, 
            size_t nmemb, 
            CurlSession* aThis
            ); 

    private:
        CURL* iCurl;

        WriteCallback iWriteCallback;

        OnProgressCallback iOnProgressCallback;
        
        std::string iHeader;
    };
}

#endif

