#include "net/HttpDownloader.h"
#include "net/CurlSession.h"
#include "utl/String.h"
#include <curl/curl.h>
#include <curl/easy.h>

namespace net
{
    /**
     * Ctor
     * @param aUrl
     * @param aLocalName
     * @param aProgressCallback
     * @param aOnFinishDownload
     */
    HttpDownloader::HttpDownloader
        (
        const std::string& aUrl,
        const std::string& aLocalName,
        const ProgressCallback& aProgressCallback,
        const OnFinishDownload& aOnFinishDownload
        )
        : utl::Logger( "HttpDownloader" )
        , iUrl( aUrl )
        , iLocalName( aLocalName )
        , iProgressCallback( aProgressCallback )
        , iOnFinishDownload( aOnFinishDownload )
        , iTotalSize( 0 )
        , iDownloadedSize( 0 )
        , iTerminated( false )
        , iStatus( STATUS_INIT )
        , iLocalFileFd( NULL )
        , iResponseCode( CurlSession::INVALID_HTTP_RESPONSE_CODE )
        , iTailPeekSize( 0 )
    {
    }

    /**
     * Dtor
     */
    HttpDownloader::~HttpDownloader()
    {
        cancelDownload();
    }

    /**
     * Start download. Execute in another thread
     */
    void HttpDownloader::startDownload
        (
        long int aTailPeekSize
        )
    {
        iStatus = STATUS_DOWNLOADING;
        iTailPeekSize = aTailPeekSize;
        iDownloadThread.reset
            (
            new boost::thread
                (
                boost::bind( &HttpDownloader::doDownload, this )
                )
            );
    }

    /**
     * Cancel download
     */
    void HttpDownloader::cancelDownload()
    {
        iTerminated = true;
        if( iDownloadThread )
        {
            iDownloadThread->join();
            iDownloadThread.reset();
        }
    }

    /**
     * Get total size
     * @return 
     */
    size_t HttpDownloader::getTotalSize() const
    {
        return iTotalSize;
    }

    /**
     * Get downloaded size
     * @return 
     */
    size_t HttpDownloader::getDownloadedSize() const
    {
        return iDownloadedSize;
    }

    /**
     * Is download complete
     * @return 
     */
    bool HttpDownloader::isFinished() const
    {
        return ( STATUS_INIT        != iStatus && 
                 STATUS_DOWNLOADING != iStatus );
    }

    /**
     * Write downloaded data through HTTP
     */
    size_t HttpDownloader::writeDownloadedData
        (
        void *ptr, 
        size_t size, 
        size_t nmemb
        ) 
    {
        size_t written;
        written = fwrite( ptr, size, nmemb, iLocalFileFd );
        return written;
    }
    
    /**
     * On HTTP download progess callback
     * @param aSrc
     * @param dltotal
     * @param dlnow
     * @param ultotal
     * @param ulnow
     * @return 
     */
    int HttpDownloader::onProgressCallback
        (
        double dltotal,
        double dlnow,
        double,// ultotal,
        double// ulnow
        )
    {
        iTotalSize = dltotal;
        iDownloadedSize = dlnow;
        iProgressCallback( iDownloadedSize, iTotalSize );
        return ( iTerminated ? -1 : 0 );
    }
    
    /**
     * Main procedure to do the download
     */
    void HttpDownloader::doDownload()
    {   
        iStatus = STATUS_DOWNLOADING;
        
        CURLcode ret;
        CurlSession curlSession;
        
        //
        // Get content length first
        //
        double contentLength; 
        ret = curlSession.performGetContentLength( iUrl, contentLength );
        if( CURLE_OK != ret || 0 == contentLength )
        {
            error( "Cannot get content length of %s", iUrl.c_str() );
            iStatus = STATUS_NO_CONTENT_LENGTH;
            iOnFinishDownload( 0, iStatus, iResponseCode );
            return;
        }
        
        // Create the download file
        iLocalFileFd = fopen( iLocalName.c_str(), "wb" );
        
        //
        // Download the tail first
        // TODO: Extract the specialized behavior
        //
        if( iTailPeekSize > 0 && contentLength > 0 )
        {
            long int tailSize = std::min( (long int)contentLength, (long int)iTailPeekSize );
            long int tailStartOffset = contentLength - tailSize;
            fseek( iLocalFileFd, tailStartOffset, SEEK_SET ); // write from tailStartOffset
            ret = curlSession.performDownload
                (
                iUrl,
                iResponseCode,
                utl::strFormat( "%ld-", tailStartOffset ),
                boost::bind( &HttpDownloader::writeDownloadedData, this, _1, _2, _3 )
                );

            if( ( CURLE_OK != ret ) || 
                ( 206 != iResponseCode && 200 != iResponseCode ) )   // 206: Partial content
            {
                fclose( iLocalFileFd );
                iStatus = STATUS_TAIL_DOWNLOAD_ERROR;
                iOnFinishDownload( 0, iStatus, iResponseCode );
                return;
            }
            
            // Double check the tail content length
            double partialContentLength = 0;
            ret = curlSession.getInfo
                (
                CURLINFO_CONTENT_LENGTH_DOWNLOAD, 
                &partialContentLength 
                );
            
            if( CURLE_OK != ret || partialContentLength != tailSize )
            {
                error
                    (
                    "Partial content length: %d != request tail length: %d. Cross the finger..",
                    partialContentLength,
                    tailSize
                    );
            }
        }
        
        //
        // Download the whole file. Override the downloaded part to simplify everything
        //
        fflush( iLocalFileFd );
        fseek( iLocalFileFd, 0, SEEK_SET );
        ret = curlSession.performDownload
            (
            iUrl,
            iResponseCode,
            "0-",
            boost::bind( &HttpDownloader::writeDownloadedData, this, _1, _2, _3 ),
            boost::bind( &HttpDownloader::onProgressCallback, this, _1, _2, _3, _4 )
            );

        fclose( iLocalFileFd );        

        if( CURLE_ABORTED_BY_CALLBACK == ret )
        {
            trace( "HTTP content download aborted" );
            iStatus = STATUS_ABORTED;
        }
        else if( CURLE_OK == ret )
        {
            if( CurlSession::INVALID_HTTP_RESPONSE_CODE == iResponseCode )
            {
                trace( "Cannot get HTTP code" );
                iStatus = STATUS_NO_RESPONSE_CODE;
            }
            else if( 200 != iResponseCode )
            {
                trace( "HTTP error code: %d", iResponseCode );
                iStatus = STATUS_RESPONSE_ERROR_CODE;
            }
            else
            {
                trace( "HTTP downloaded!" );
                iStatus = STATUS_SUCCESS;
            }
        }

        iOnFinishDownload( iDownloadedSize, iStatus, iResponseCode );
    }
    
    /**
     * Get local name
     * @return 
     */
    std::string HttpDownloader::getLocalName() const 
    {
        return iLocalName; 
    }

    /**
     * Get download URL
     * @return 
     */
    std::string HttpDownloader::getUrl() const 
    { 
        return iUrl; 
    }

    /**
     * Get download status
     * @return 
     */
    HttpDownloader::Status HttpDownloader::getDownloadStatus() const
    {
        return iStatus;
    }

    /**
     * Get response code
     * @return 
     */
    int HttpDownloader::getResponseCode() const
    {
        return iResponseCode;
    }

    /**
     * Peek the content length before download
     * @return 
     */
    int64_t HttpDownloader::peekContentLength() const
    {
        CURLcode ret;
        CurlSession curlSession;
        
        //
        // Get content length first
        //
        double contentLength; 
        ret = curlSession.performGetContentLength( iUrl, contentLength );
        if( CURLE_OK != ret )
        {
            return -1;
        }

        return contentLength;
    }

}

