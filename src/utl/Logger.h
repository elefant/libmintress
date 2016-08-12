#ifndef UTL_LOGGER_H
#define UTL_LOGGER_H

#include "Loggable.h"

#ifndef UTL_LOGGER_USE_NULL_LOGGER
#define UTL_LOGGER_USE_NULL_LOGGER 0
#endif

#ifndef UTL_LOGGER_LOG_TO_FILE
#define UTL_LOGGER_LOG_TO_FILE 1
#endif

#ifndef UTL_LOGGER_LOG_TO_STDOUT
#define UTL_LOGGER_LOG_TO_STDOUT 1
#endif

namespace utl
{
#if( UTL_LOGGER_USE_NULL_LOGGER )
    class Logger : public LoggerNull
    {
        Logger
            (
            const std::string& aName,
            EnableType aEnabledFlag = ENABLE_ALL
            ) 
            : LoggerNull( aName, aEnabledFlag ) 
        {
        }
    };
#else
    class Logger : public Loggable
    {
    public:
        struct StaticData
        {
        public:
            StaticData();

            ~StaticData();
            
        #if( UTL_LOGGER_LOG_TO_FILE )
            FILE* iLogFp;
        #endif
        };

    public:
        Logger
            (
            const std::string& aName,
            EnableType aEnabledFlag  = ENABLE_ALL
            );

        void trace
            (
            const char* aFormat, 
            ...
            ) const;
        
        void warning
            (
            const char* aFormat, 
            ...
            ) const;
                
        void error
            (
            const char* aFormat, 
            ...
            ) const;

        void assert
            (
            bool aStatement,
            const char* aFormat, 
            ...
            ) const;

    public:
        static void setUseDeLogger
            (
            bool aUseDeLogger
            );

    private:
        void doLog
            (
            LogType aLogType,
            const char* aFormat,
            const va_list& aVaList
            ) const;

        void logToDeLogger
            (
            LogType aLogType,
            const std::string& aMsg
            ) const;
        
    private:
        std::string iName;

        EnableType iEnabledFlag;

        static StaticData sStaticData;

        static bool sUseDeLogger;
    };
#endif

    void trace
        (
        const char* aFormat, 
        ...
        );

    void warning
        (
        const char* aFormat, 
        ...
        );
        
    void error
        (
        const char* aFormat, 
        ...
        );

    void assert
        (
        bool aStatement,
        const char* aFormat, 
        ...
        );
    
}

#endif

