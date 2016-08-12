#ifndef MMF_TRANSCODER_ENGINE_VLC_H
#define MMF_TRANSCODER_ENGINE_VLC_H

#include "TranscoderEngine.h"
#include "utl/Logger.h"

/**
 * S/W transcoder through ffmpeg
 */
namespace mmf
{
    class TranscoderEngineVlc : public TranscoderEngine, private utl::Logger
    {
    public:
        TranscoderEngineVlc
            (
            const Config& aConfig,
            int aOutputPort
            );

        virtual ~TranscoderEngineVlc();

        virtual bool start
            (
            OnFinishCallbackType aOnFinishCallback = OnFinishCallbackType()
            );

        virtual std::string getLog() const;
        
    private:
        void killRunningProcess();

        bool isRunning() const;

    private:
        static const std::string sLaunchCommandTemplate;

        int   iOutputPort;

        pid_t iRunningPid;

        std::string iLaunchCommand;
    };
}

#endif

