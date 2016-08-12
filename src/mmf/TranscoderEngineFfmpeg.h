#ifndef MMF_TRANSCODER_ENGINE_FFMPEG_H
#define MMF_TRANSCODER_ENGINE_FFMPEG_H

#include "TranscoderEngine.h"
#include "utl/Logger.h"

/**
 * S/W transcoder through ffmpeg
 */
namespace mmf
{
    class TranscoderEngineFfmpeg : public TranscoderEngine, private utl::Logger
    {
    public:
        TranscoderEngineFfmpeg
            (
            const Config& aConfig,
            int aOutputPort,
            const std::string& aLogPath
            );

        virtual ~TranscoderEngineFfmpeg();

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
        
        std::string iLogPath;
    };
}

#endif

