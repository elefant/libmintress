#ifndef MMF_TRANSCODER_ENGINE_H
#define MMF_TRANSCODER_ENGINE_H

#include <string>
#include <boost/function.hpp>

namespace mmf
{
    /**
    * \brief TrancoderEngine
    * A interface of transcoder since it might be implemented by hardware
    */
    class TranscoderEngine
    {
    public:
        typedef boost::function<void( int, int )> OnFinishCallbackType;
        
    public:
        struct Config
        {
            int iWidth;
            int iHeight;
            int iFrameRate;
            int iKeyFrameInt;
            int iBitrate;
            double iPosition;
            bool iNativeFramerateUsed;
            bool iDisableVideo;
            bool iIsInputImage;
            int iIsImageDuration;

            std::string iMediaFileName;

            Config();
        };

    public:
        TranscoderEngine
            (
            const Config& Config
            );

        virtual ~TranscoderEngine() {}

        virtual bool start
            (
            OnFinishCallbackType aOnFinishCallback = OnFinishCallbackType()
            ) = 0;
        
        virtual std::string getLog() const = 0;

    protected:
        Config iConfig; // Common config for all transcoder engine

        OnFinishCallbackType iOnFinishCallback;
    };

    // If you want to implement other transcoder e.g. hardware,
    // Create another class as follows

    //class TranscoderEngineHw : public TranscoderEngine
    //{
    //public:
    //    TranscoderEngineHw
    //        (
    //        const Config& aConfig
    //        );
    //
    //    virtual ~TranscoderEngineHw();
    //
    //    virtual bool start() = 0;
    //};
}

#endif

