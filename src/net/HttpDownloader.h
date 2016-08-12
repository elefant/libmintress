#ifndef NET_HTTP_DOWNLOADER_H
#define NET_HTTP_DOWNLOADER_H

#include <boost/function.hpp>
#include <string>
#include <boost/thread.hpp>
#include "utl/Logger.h"

namespace net
{
    class HttpDownloader : private utl::Logger
    {
    public:
        enum Status
        {
            STATUS_INIT,
            STATUS_DOWNLOADING,
            STATUS_SUCCESS,
            STATUS_ABORTED,
            STATUS_RESPONSE_ERROR_CODE,
            STATUS_NO_RESPONSE_CODE,
            STATUS_TAIL_DOWNLOAD_ERROR,
            STATUS_NO_CONTENT_LENGTH,
        };
        
        typedef boost::function<void
            (
            size_t aDownloadedSize, 
            size_t aTotalSize 
            )> ProgressCallback;
        
        typedef boost::function<void( size_t, Status, int )> OnFinishDownload;
    
    public:
        HttpDownloader
            (
            const std::string& aUrl,
            const std::string& aLocalName,
            const ProgressCallback& aProgressCallback = ProgressCallback(),
            const OnFinishDownload& aOnFinishDowload = OnFinishDownload()
            );

        ~HttpDownloader();

        void startDownload
            (
            long int aTailPeekSize = 0
            );

        void cancelDownload();

        size_t getTotalSize() const;

        size_t getDownloadedSize() const;

        bool isFinished() const;

        Status getDownloadStatus() const;

        std::string getLocalName() const;

        std::string getUrl() const;

        int getResponseCode() const;

        int64_t peekContentLength() const;

    private:
        void doDownload();

        size_t writeDownloadedData
            (
            void *ptr, 
            size_t size, 
            size_t nmemb
            );

        int onProgressCallback
            (
            double dltotal,
            double dlnow,
            double ultotal,
            double ulnow
            );

    private:
        std::string iUrl;

        std::string iLocalName;

        ProgressCallback iProgressCallback;

        OnFinishDownload iOnFinishDownload;
        
        size_t iTotalSize;
        
        size_t iDownloadedSize;

        bool iTerminated;

        boost::shared_ptr<boost::thread> iDownloadThread;

        Status iStatus;

        FILE* iLocalFileFd;

        long iResponseCode;
        
        long int iTailPeekSize;
    };

};

#endif

