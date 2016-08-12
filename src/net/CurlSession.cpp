#include "CurlSession.h"
#include <curl/easy.h>

namespace net
{
    /**
     * Ctor
     */
    CurlSession::CurlSession()
        : iCurl( NULL )
    {
    }
    
    /**
     * Dtor
     */
    CurlSession::~CurlSession()
    {
        if( NULL != iCurl )
        {
            curl_easy_cleanup( iCurl );
        }
    }

    /**
     * Perform get content length only
     * @param aUrl
     * @param aContentLength
     * @return 
     */
    CURLcode CurlSession::performGetContentLength
        (
        const std::string& aUrl,
        double& aContentLength,
        int aTimeoutMs
        )
    {
        if( NULL != iCurl )
        {
            curl_easy_cleanup( iCurl );
        }
        iCurl = curl_easy_init();

        // Setup curl easy option
        curl_easy_setopt( iCurl, CURLOPT_URL, aUrl.c_str() );
        curl_easy_setopt( iCurl, CURLOPT_NOBODY, 1 );
        curl_easy_setopt( iCurl, CURLOPT_TIMEOUT_MS, aTimeoutMs );

        // Perform!
        CURLcode curlRet = curl_easy_perform( iCurl );
        if( CURLE_OK != curlRet )
        {
            return curlRet;
        }

        // Check if response code == 200
        long responseCode = INVALID_HTTP_RESPONSE_CODE;
        curlRet = getInfo( CURLINFO_RESPONSE_CODE, &responseCode );
        if( CURLE_OK != curlRet )
        {
            return curlRet;
        }
        if( 200 != responseCode )
        {
            return CURLE_HTTP_RETURNED_ERROR;
        }

        // Get content length
        curlRet = getInfo( CURLINFO_CONTENT_LENGTH_DOWNLOAD, &aContentLength );
        if( CURLE_OK != curlRet )
        {
            return curlRet;
        }

        return CURLE_OK;
    }

    /**
     * Perform get header only
     * @param aUrl
     * @return 
     */
    CURLcode CurlSession::performGetHeader
        (
        const std::string& aUrl
        )
    {
        if( NULL != iCurl )
        {
            curl_easy_cleanup( iCurl );
        }
        iCurl = curl_easy_init();

        // Setup curl easy option
        curl_easy_setopt( iCurl, CURLOPT_URL, aUrl.c_str() );
        curl_easy_setopt( iCurl, CURLOPT_HEADERFUNCTION, writeHeader );
        curl_easy_setopt( iCurl, CURLOPT_WRITEHEADER, this );
        curl_easy_setopt( iCurl, CURLOPT_NOBODY, 1 );

        iHeader = "";
        
        // Perform!
        CURLcode curlRet = curl_easy_perform( iCurl );

        return curlRet;
    }

    /**
     * Perform downloading
     * @param aUrl
     * @param aResponseCode
     * @param aRange
     * @param aWriteCallback
     * @param aOnProgressCallback
     * @return 
     */
    CURLcode CurlSession::performDownload
        (
        const std::string& aUrl,
        long& aResponseCode,
        const std::string& aRange,
        const WriteCallback& aWriteCallback,
        const OnProgressCallback& aOnProgressCallback
        )
    {
        if( NULL != iCurl )
        {
            curl_easy_cleanup( iCurl );
        }
        iCurl = curl_easy_init();

        bool rangedDownload = ( aRange != "0-" );
        // Setup curl easy option
        curl_easy_setopt( iCurl, CURLOPT_URL, aUrl.c_str() );
        curl_easy_setopt( iCurl, CURLOPT_WRITEFUNCTION, writeCallback );
        curl_easy_setopt( iCurl, CURLOPT_WRITEDATA, this );
        curl_easy_setopt( iCurl, CURLOPT_NOPROGRESS, 0 );
        curl_easy_setopt( iCurl, CURLOPT_PROGRESSFUNCTION, onProgressCallback );
        curl_easy_setopt( iCurl, CURLOPT_PROGRESSDATA, this );
        if( rangedDownload )
        {
            curl_easy_setopt( iCurl, CURLOPT_RANGE, aRange.c_str() );
        }
            
        iWriteCallback = aWriteCallback;
        iOnProgressCallback = aOnProgressCallback;
        
        // Perform!
        CURLcode curlRet = curl_easy_perform( iCurl );
        if( CURLE_OK != curlRet )
        {
            aResponseCode = INVALID_HTTP_RESPONSE_CODE;
            return curlRet;
        }

        // Check if response code == 200
        curlRet = getInfo( CURLINFO_RESPONSE_CODE, &aResponseCode );
        if( CURLE_OK != curlRet )
        {
            aResponseCode = INVALID_HTTP_RESPONSE_CODE;
            return curlRet;
        }
        if( (  rangedDownload && 206 != aResponseCode ) ||
            ( !rangedDownload && 200 != aResponseCode ) )
        {
            return CURLE_HTTP_RETURNED_ERROR;
        }
        
        return CURLE_OK;
    }

    /**
     * Get info
     * @param aWhatInfo
     * @param aInfo
     * @return 
     */
    CURLcode CurlSession::getInfo
        (
        CURLINFO aWhatInfo,
        void* aInfo
        )
    {
        return curl_easy_getinfo( iCurl, aWhatInfo, aInfo );
    }

    /**
     * Wrtie callback
     * @param ptr
     * @param size
     * @param nmemb
     * @param aThis
     * @return 
     */
    size_t CurlSession::writeCallback
        (
        void *ptr, 
        size_t size, 
        size_t nmemb, 
        CurlSession* aThis
        )
    {
        if( !aThis->iWriteCallback.empty() )
        {
            return aThis->iWriteCallback( ptr, size, nmemb );
        }
        return size * nmemb;
    }

    /**
     * On progress callback
     * @param aThis
     * @param dltotal
     * @param dlnow
     * @param ultotal
     * @param ulnow
     * @return 
     */
    int CurlSession::onProgressCallback
        (
        CurlSession* aThis,
        double dltotal,
        double dlnow,
        double ultotal,
        double ulnow
        )
    {
        if( !aThis->iOnProgressCallback.empty() )
        {
            return aThis->iOnProgressCallback( dltotal, dlnow, ultotal, ulnow );
        }
        return 0; // continue;
    }

    /**
     * Wrtie header callback
     * @param ptr
     * @param size
     * @param nmemb
     * @param aThis
     * @return 
     */
    size_t CurlSession::writeHeader
        (
        void *ptr, 
        size_t size, 
        size_t nmemb, 
        CurlSession* aThis
        )
    {
        aThis->iHeader += std::string( (char*)ptr, size * nmemb );
        return size * nmemb;
    }

}

