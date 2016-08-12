#ifndef UTL_LOGGABLE_H
#define UTL_LOGGABLE_H

#include <string>
#include <stdarg.h>

#ifdef assert
#undef assert
#endif

#ifdef LOG_WARNING
#undef LOG_WARNING
#endif

#define LOG_TYPE_TO_ENABLE( type ) ( 1 << ( type ) )  
#define LOG_TYPE_IS_ENABLED( flag , type )  ( 0 != ( ( flag ) & LOG_TYPE_TO_ENABLE( type ) ) )

namespace utl
{
    /**
     * A null-implemented Logger
     */
    class Loggable 
    {
    public:
        enum LogType
        {
            LOG_TRACE,
            LOG_WARNING,
            LOG_ERROR,
            LOG_ASSERT,
            LOG_TYPE_CNT,
        };

        enum EnableType
        {
            ENABLE_TRACE   = LOG_TYPE_TO_ENABLE( LOG_TRACE ),
            ENABLE_WARNING = LOG_TYPE_TO_ENABLE( LOG_WARNING ),
            ENABLE_ERROR   = LOG_TYPE_TO_ENABLE( LOG_ERROR ),
            ENABLE_ASSERT  = LOG_TYPE_TO_ENABLE( LOG_ASSERT ),

            ENABLE_ALL  = 0xFFFFFFFF,
            ENABLE_NONE = 0,
        };

    protected:
        Loggable
            (
            const std::string&, /* aName */ 
            EnableType //aEnabledFlag
            ) 
        {
        }
        
    public:
        virtual ~Loggable() {}

        //
        // All the interfaces are intentionally non-virtual to make them inline-able
        //

        void trace
            (
            const char*, //aFormat, 
            ...
            ) const
        {
        }

        void assert
            ( 
            bool, // aStatement,
            const char* //aFormat, 
            ...
            ) const
        {
        }
        
        void error
            (
            const char* , //aFormat, 
            ...
            ) const
        {
        }
        
        void warning
            (
            const char* ,//aFormat, 
            ...
            ) const
        {
        }
        
        //
        // Static function
        //
        static std::string getLogDir();

        static std::string getLogName();

    };
    
    /**
     * NullLogger
     */
    class LoggerNull : public Loggable
    {
    protected:
        LoggerNull
            (
            const std::string& aName, 
            EnableType aEnabledFlag
            )
            : Loggable( aName, aEnabledFlag )
        {
        }
    };
}

#endif

