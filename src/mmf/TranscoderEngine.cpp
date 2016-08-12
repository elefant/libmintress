#include "TranscoderEngine.h"

namespace mmf
{
    /**
     * Constructor of TranscoderEngine
     * @param aParameter
     */
    TranscoderEngine::TranscoderEngine
        (
        const Config& aConfig
        )
        : iConfig( aConfig )
    {
    }

    /**
     * Default ctor of TranscoderEngine::Config
     */
    TranscoderEngine::Config::Config()
        : iWidth( 0 )
        , iHeight( 0 )
        , iFrameRate( 0 )
        , iKeyFrameInt( 1 )
        , iBitrate( 0 )
        , iPosition( 0.0f )
        , iNativeFramerateUsed( false )
        , iDisableVideo( false )
        , iIsInputImage( false )
        , iIsImageDuration( 0 )
    {
    }
}

