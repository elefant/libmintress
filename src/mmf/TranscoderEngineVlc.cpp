#include "TranscoderEngineVlc.h"
#include "TranscoderEngine.h"
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include "sys/Shell.h"
#include "utl/String.h"
#include <fstream>
#include <vector>

namespace mmf
{
    #define KEYWORD_SOURCE        "{source}"
    #define KEYWORD_BITRATE       "{bitrate}"
    #define KEYWORD_WIDTH         "{width}"
    #define KEYWORD_HEIGHT        "{height}"
    #define KEYWORD_FRAME_RATE    "{frames}"
    #define KEYWORD_IFRAME_INT    "{iframe}"

    #define KEYWORD_TRANSCODER_TARGET_PORT "{transcoder_target_port}"

    const std::string TranscoderEngineVlc::sLaunchCommandTemplate =
        "cvlc "
        "\"" KEYWORD_SOURCE "\" " 
        "--sout="
        "'#transcode{vcodec=h264,venc=x264{bframes=0,qp-max=51,qp-min=10,keyint=" KEYWORD_IFRAME_INT "}"
        ",vb=" KEYWORD_BITRATE
        ",fps=" KEYWORD_FRAME_RATE 
        ",width=" KEYWORD_WIDTH
        ",height=" KEYWORD_HEIGHT
        ",acodec=aac,samplerate=48000,channels=2,ab=192000}"
        ":std{mux=ts,access=udp"
        ",dst=127.0.0.1:" KEYWORD_TRANSCODER_TARGET_PORT "}'";

    /**
     * Constructor
     * @param aParameter
     */
    TranscoderEngineVlc::TranscoderEngineVlc
        (
        const Config& aConfig,
        int aOutputPort
        )
        : TranscoderEngine( aConfig )
        , utl::Logger( "vlc" )
        , iOutputPort( aOutputPort )
        , iRunningPid( 0 )
    {
    }

    /**
     * Destructor
     */
    TranscoderEngineVlc::~TranscoderEngineVlc()
    {
        killRunningProcess();
    }

    /**
     * Kill the running process. It's safe even if there is no
     * running process
     */
    void TranscoderEngineVlc::killRunningProcess()
    {
        if( isRunning() )
        {
            sys::shell::terminate( iRunningPid );
            iRunningPid = 0;
        }
    }

    /**
     * Is the trancoder running
     * @return
     */
    bool TranscoderEngineVlc::isRunning() const
    {
        return ( 0 != iRunningPid );
    }

    /**
     * Launch vlc from command line
     */
    bool TranscoderEngineVlc::start
        (
        OnFinishCallbackType aOnFinishCallback
        )
    {
        killRunningProcess();

        std::string launchCommand = sLaunchCommandTemplate;

        // Input media file name. It may be a local file or live streaming
        boost::replace_all( launchCommand, KEYWORD_SOURCE, iConfig.iMediaFileName );

        // Width
        boost::replace_all( launchCommand, KEYWORD_WIDTH, utl::intTostr( iConfig.iWidth ) );

        // Height
        boost::replace_all( launchCommand, KEYWORD_HEIGHT, utl::intTostr( iConfig.iHeight ));

        // Frames ?
        boost::replace_all( launchCommand, KEYWORD_FRAME_RATE, utl::intTostr( iConfig.iFrameRate ));

        // Bit rate
        boost::replace_all( launchCommand, KEYWORD_BITRATE, utl::intTostr( iConfig.iBitrate ) );

        // I Frame ?
        boost::replace_all( launchCommand, KEYWORD_IFRAME_INT, utl::intTostr( iConfig.iKeyFrameInt ));

        // Output port
        boost::replace_all( launchCommand, KEYWORD_TRANSCODER_TARGET_PORT, utl::intTostr( iOutputPort ) );
        
        iRunningPid = sys::shell::launch
            (
            launchCommand,
            "",
            aOnFinishCallback 
            );

        trace( "TransocderPid:%d   %s", iRunningPid, launchCommand.c_str() );

        return isRunning();
    }
    
    /**
     * Get log
     * @return 
     */
    std::string TranscoderEngineVlc::getLog() const
    {
        return "";
    }
}

