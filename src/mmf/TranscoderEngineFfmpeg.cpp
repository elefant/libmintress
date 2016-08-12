#include "TranscoderEngineFfmpeg.h"
#include "TranscoderEngine.h"
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include "sys/Shell.h"
#include "utl/String.h"
#include "net/Misc.h"
#include <fstream>
#include <vector>

namespace
{
    enum { MAX_LOG_STRING_LENGTH = 50 * 1024 }; // 50 KB
}

namespace mmf
{
    #define KEYWORD_SOURCE        "{source}"
    #define KEYWORD_BITRATE       "{bitrate}"
    #define KEYWORD_POSITION      "{postion}"
    #define KEYWORD_FRAME_RATE    "{frames}"
    #define KEYWORD_IFRAME_INT    "{iframe}"
    #define KEYWORD_OPTIONS       "{options}"
    #define KEYWORD_DISABLE_VIDEO "{disable_video}"
    #define KEYWORD_DIMENSTION    "{dimension}"

    #define KEYWORD_TRANSCODER_TARGET_PORT "{transcoder_target_port}"

    const std::string TranscoderEngineFfmpeg::sLaunchCommandTemplate =
        "ffmpeg"
        " -loglevel info -nostats"
        " " KEYWORD_OPTIONS
        " " KEYWORD_POSITION
        " -i " "\"" KEYWORD_SOURCE "\""
        " " KEYWORD_DIMENSTION
        " -g " KEYWORD_IFRAME_INT
        " -r " KEYWORD_FRAME_RATE
        " -vcodec libx264 -bf 0 -b " KEYWORD_BITRATE
        " -flags +loop -cmp +chroma -partitions +parti4x4+partp8x8+partb8x8 -subq 5"
        " -trellis 1 -refs 1 -coder 1 -me_range 16 -keyint_min 25 -sc_threshold 40"
        " -i_qfactor 0.71 -bt 200k -maxrate 2048k -bufsize 2048k"
        " -rc_eq 'blurCplx^(1-qComp)' -qcomp 0.6 -qmin 10 -qmax 51 -qdiff 4"
        " -level 30 -bufsize 3M"
        " -acodec libfaac -ac 2 -ar 48000 -ab 192k"
        " -f mpegts " KEYWORD_DISABLE_VIDEO " udp://127.0.0.1:" KEYWORD_TRANSCODER_TARGET_PORT;

    /**
     * Constructor
     * @param aParameter
     */
    TranscoderEngineFfmpeg::TranscoderEngineFfmpeg
        (
        const Config& aConfig,
        int aOutputPort,
        const std::string& aLogPath
        )
        : TranscoderEngine( aConfig )
        , utl::Logger( "ffmpeg" )
        , iOutputPort( aOutputPort )
        , iRunningPid( 0 )
        , iLogPath( aLogPath )
    {
    }

    /**
     * Destructor
     */
    TranscoderEngineFfmpeg::~TranscoderEngineFfmpeg()
    {
        killRunningProcess();
    }

    /**
     * Kill the running process. It's safe even if there is no
     * running process
     */
    void TranscoderEngineFfmpeg::killRunningProcess()
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
    bool TranscoderEngineFfmpeg::isRunning() const
    {
        return ( 0 != iRunningPid );
    }

    /**
     * Launch ffmpeg from command line
     */
    bool TranscoderEngineFfmpeg::start
        (
        OnFinishCallbackType aOnFinishCallback
        )
    {
        killRunningProcess();

        std::string launchCommand = sLaunchCommandTemplate;

        // Input media file name. It may be a local file or live streaming
        boost::replace_all( launchCommand, KEYWORD_SOURCE, iConfig.iMediaFileName );

        // Start position
        std::string postion = boost::str
            (
            boost::format( "-ss %.2lf" ) % iConfig.iPosition
            );
        boost::replace_all( launchCommand, KEYWORD_POSITION, postion );

        // Dimension
        std::string dimenstionOption;
        if( !iConfig.iIsInputImage )
        {
            // Fixed width and height
            dimenstionOption = utl::strFormat( "-s %dx%d", iConfig.iWidth, iConfig.iHeight );
        }
        else
        {
            // Fixed width and rounded aspect height
            dimenstionOption = utl::strFormat( "-vf \"scale=trunc(oh/a/2)*2:%d\"", iConfig.iWidth );
        }
        boost::replace_all( launchCommand, KEYWORD_DIMENSTION, dimenstionOption );

        // Frames ?
        boost::replace_all( launchCommand, KEYWORD_FRAME_RATE, utl::intTostr( iConfig.iFrameRate ));

        // I Frame ?
        boost::replace_all( launchCommand, KEYWORD_IFRAME_INT, utl::intTostr( iConfig.iKeyFrameInt ));

        // Bit rate
        boost::replace_all( launchCommand, KEYWORD_BITRATE, utl::intTostr( iConfig.iBitrate ) );

        // Output port
        boost::replace_all( launchCommand, KEYWORD_TRANSCODER_TARGET_PORT, utl::intTostr( iOutputPort ) );

        // Disable video or not
        boost::replace_all( launchCommand, KEYWORD_DISABLE_VIDEO, iConfig.iDisableVideo ? "-vn" : "" );

        // Options
        std::vector<std::string> optionList;
        if( iConfig.iNativeFramerateUsed )
        {
            optionList.push_back( "-re" );
        }
        
        //
        // Special case! Axis camera stream with MJPEG video
        // TODO: Determine in StreamingSource and find a proper framerate 
        //
        std::string mediaName = iConfig.iMediaFileName;
        boost::algorithm::to_lower( mediaName ); 
        if( net::PROTOCOL_HTTP == net::getProtocol( mediaName ) &&
            std::string::npos != iConfig.iMediaFileName.find( "axis-cgi" ) &&
            std::string::npos != iConfig.iMediaFileName.find( "mjpg" ) )
        {
            optionList.push_back( "-f mjpeg" );
            optionList.push_back( "-r 15" );
        }

        if( iConfig.iIsInputImage )
        {
            optionList.push_back( "-loop 1" );
            optionList.push_back( utl::strFormat( "-t %d", iConfig.iIsImageDuration ) );
        }
        else
        {
            optionList.push_back( "-async 1" );
        }
        
        // Convert optionList to string
        std::string option;
        for( size_t i = 0; i < optionList.size(); i++ )
        {
            option += utl::strFormat( " %s ", optionList[i].c_str() );
        }
        boost::replace_all( launchCommand, KEYWORD_OPTIONS, option );
        
        iRunningPid = sys::shell::launch
            (
            launchCommand,
            iLogPath,
            aOnFinishCallback 
            );

        trace( "TransocderPid:%d   %s", iRunningPid, launchCommand.c_str() );

        return isRunning();
    }
    
    /**
     * Get log
     * @return 
     */
    std::string TranscoderEngineFfmpeg::getLog() const
    {
        std::ifstream logFile( iLogPath.c_str() );
        if( logFile )
        {
            logFile.seekg( 0, std::ios::end );
            std::streampos logStringLength = logFile.tellg();
            logStringLength = std::min( logStringLength, (std::streampos)MAX_LOG_STRING_LENGTH );
            logFile.seekg( -logStringLength, std::ios::end );
            
            std::vector<char> buffer( logStringLength );
            logFile.read( &buffer[0], buffer.size() );
            logFile.close();
            
            return &buffer[0];
        }
        
        return "";
    }
}

