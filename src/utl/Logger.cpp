#include <vector>
#include <stdio.h>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/conversion.hpp>
#include <stdio.h>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include "Logger.h"
#include "LoggerC.h"
#include "String.h"
#include "sys/Application.h"
#include <DeLogger.h>

namespace
{
    void removeOldLogs();
    
    const char* LOG_NAME_FORMAT = "%Y-%m%d-%H%M-%S.txt";
    
    enum 
    { 
        LOG_FILE_KEEP_DAYS = 7,
        LOG_FILE_KEEP_HOURS = LOG_FILE_KEEP_DAYS * 24, 
    };
}

namespace utl
{
    static Logger gGeneralLogger = Logger( "General" ); // the general logger

#if( !UTL_LOGGER_USE_NULL_LOGGER )

    /**
     * Open the log file
     * TODO: Move the utl::time::getNowString()
     */
    static std::string getNowString()
    {
        boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
        std::string nowString = boost::posix_time::to_iso_extended_string( now );
        return nowString;
    }

    Logger::StaticData Logger::sStaticData;

    bool Logger::sUseDeLogger = true;

    /**
     * Open the log file
     */
    Logger::StaticData::StaticData()
    {
    #if( UTL_LOGGER_LOG_TO_FILE )
        try
        {
            removeOldLogs();
            
            boost::filesystem::create_directories( getLogDir() );
            iLogFp = fopen( ( getLogDir() + getLogName() ).c_str(), "w" );
            std::string symlinkName = getLogDir() + "latest.txt";
            unlink( symlinkName.c_str() );
            symlink( ( getLogDir() + getLogName() ).c_str(), symlinkName.c_str() );
            setlinebuf( iLogFp );
            const char* START_STRING = 
                "//-------------------------------------------------------\n"
                "// Log starts @ %s\n"
                "//-------------------------------------------------------\n";

            if( iLogFp )
            {
                fprintf( iLogFp, START_STRING, getNowString().c_str() );
            }
        }
        catch( const boost::filesystem::filesystem_error& e )
        {
            fprintf
                (
                stderr,
                "Cannot create log directory %s: %s\n. Run w/o log.\n",
                getLogDir().c_str(),
                e.code().message().c_str()
                );
            return;
        }
    #endif
    }

    /**
     * Close the log file
     */
    Logger::StaticData::~StaticData()
    {
    #if( UTL_LOGGER_LOG_TO_FILE )
        const char* END_STRING = 
            "//-------------------------------------------------------\n"
            "// Log ends @ %s\n"
            "//-------------------------------------------------------\n"
            "\n"
            "\n";

        if( iLogFp )
        {
            fprintf( iLogFp, END_STRING, getNowString().c_str() );
            fclose( iLogFp );
        }
    #endif
    }

    /**
     * Ctor
     * @param aName
     */
    Logger::Logger
        (
        const std::string& aName,
        EnableType aEnabledFlag
        )
        : Loggable( aName, aEnabledFlag )
        , iName( aName )
        , iEnabledFlag( aEnabledFlag )
    {
        boost::replace_all( iName, "|", "_" );
        setlinebuf( stdout );
    }

    void Logger::setUseDeLogger
        (
        bool aUseDeLogger
        )
    {
        sUseDeLogger = aUseDeLogger;
    }

    void Logger::logToDeLogger
        (
        LogType aLogType,
        const std::string& aMsg
        ) const
    {
        DeLogger& deLogger = DeLogger::getRootLogger();
        std::string deLoggerMsg = iName + "|" + aMsg;
        switch( aLogType )
        {
            case LOG_TRACE:   
                deLogger.trace( deLoggerMsg );
                break;

            case LOG_WARNING:  
                deLogger.warn( deLoggerMsg );
                break;

            case LOG_ERROR:   
                deLogger.error( deLoggerMsg );
                break;

            case LOG_ASSERT:  
                deLogger.fatal( deLoggerMsg );
                break;

            default: 
                break;
        }
    }

    
    /**
     * The only one point to output the message
     * @param aLogType
     * @param aFormat
     * @param aVaList
     */
    void Logger::doLog
        (
        LogType aLogType,
        const char* aFormat,
        const va_list& aVaList
        ) const
    {
        if( !LOG_TYPE_IS_ENABLED( iEnabledFlag, aLogType ) )
        {
            return;
        }
    
        std::string msg = strFormat( aFormat, aVaList );

        if( sUseDeLogger )
        {
            logToDeLogger( aLogType, msg );
            return;  
        }

        std::string typeString;
        switch( aLogType )
        {
            case LOG_TRACE:   
                typeString = "TRACE";
                break;

            case LOG_WARNING: 
                typeString = "WARNING";  
                break;

            case LOG_ERROR:   
                typeString = "ERROR"; 
                break;

            case LOG_ASSERT:  
                typeString = "ASSERT";
                break;

            default: break;
        }
                
        std::string formattedLog = strFormat
            (
            "%s|%s|%s|%s\n",
            getNowString().c_str(),
            iName.c_str(),
            typeString.c_str(),
            msg.c_str()    
            );

    #if( UTL_LOGGER_LOG_TO_STDOUT )
        std::cout << formattedLog;
    #endif

    #if( UTL_LOGGER_LOG_TO_FILE )
        if( sStaticData.iLogFp )
        {
            fprintf( sStaticData.iLogFp, "%s", formattedLog.c_str() );
        }
    #endif
    }
    
    /**
     * Trace
     * @param aFormat
     * @param aVaList
     */
    void Logger::trace
        (
        const char* aFormat, 
        ...
        ) const
    {
        va_list aVaList;
        va_start( aVaList, aFormat );
        va_end( aVaList );
        doLog( LOG_TRACE, aFormat, aVaList );
    }
    
    /**
     * Warning
     * @param aFormat
     * @param aVaList
     */
    void Logger::warning
        (
        const char* aFormat, 
        ...
        ) const
    {
        va_list aVaList;
        va_start( aVaList, aFormat );
        va_end( aVaList );
        doLog( LOG_WARNING, aFormat, aVaList );
    }

    /**
     * error
     * @param aFormat
     * @param aVaList
     */
    void Logger::error
        (
        const char* aFormat, 
        ...
        ) const
    {
        va_list aVaList;
        va_start( aVaList, aFormat );
        va_end( aVaList );
        doLog( LOG_ERROR, aFormat, aVaList );
    }

    /**
     * Assert
     * @param aFormat
     * @param aVaList
     */
    void Logger::assert
        (
        bool aStatement,
        const char* aFormat, 
        ...
        ) const
    {        
        if( false == aStatement )
        {
            va_list aVaList;
            va_start( aVaList, aFormat );
            va_end( aVaList );
            doLog( LOG_ASSERT, aFormat, aVaList );
        }
    }
    
    /**
     * Get log directory
     * @return 
     */
    std::string Loggable::getLogDir()
    {
        return utl::strFormat
            (
            "%s/../log/%s/",
            sys::application::getExeDir().c_str(),
            sys::application::getExeName().c_str()
            );
    }
    
    /**
     * Get log name
     * @return 
     */
    std::string Loggable::getLogName()
    {
        time_t t = time( NULL );
        struct tm tm = *localtime( &t );
        char logFileName[1024] = { 0 };
        strftime( logFileName, sizeof( logFileName ), LOG_NAME_FORMAT, &tm );
        
        return logFileName;
    }
    
#endif

    //------------------------------------------------------
    // UTL APIs
    //------------------------------------------------------

    /**
     * Trace
     * @param aFormat
     * @param ...
     */
    void trace
        (
        const char* aFormat, 
        ...
        )
    {
        va_list aVaList;
        va_start( aVaList, aFormat );
        va_end( aVaList );
        gGeneralLogger.trace( strFormat( aFormat, aVaList ).c_str() );
    }

    /**
     * Warning
     * @param aFormat
     * @param ...
     */
    void warning
        (
        const char* aFormat, 
        ...
        )
    {
        va_list aVaList;
        va_start( aVaList, aFormat );
        va_end( aVaList );
        gGeneralLogger.warning( strFormat( aFormat, aVaList ).c_str() );
    }
        
    /**
     * Error
     * @param aFormat
     * @param ...
     */
    void error
        (
        const char* aFormat, 
        ...
        )
    {
        va_list aVaList;
        va_start( aVaList, aFormat );
        va_end( aVaList );
        gGeneralLogger.error( strFormat( aFormat, aVaList ).c_str() );
    }

    /**
     * Assert
     * @param aStatement
     * @param aFormat
     * @param ...
     */
    void assert
        (
        bool aStatement,
        const char* aFormat, 
        ...
        )
    {
        va_list aVaList;
        va_start( aVaList, aFormat );
        va_end( aVaList );
        gGeneralLogger.assert( aStatement, strFormat( aFormat, aVaList ).c_str() );
    }
}

//-----------------------------------------------------
// A C-version log functions
//-----------------------------------------------------

using namespace utl;

/**
 * trace
 * @param aFormat
 * @param ...
 */
void utl_trace
    (
    const char* aFormat, 
    ...
    )
{
    va_list aVaList;
    va_start( aVaList, aFormat );
    va_end( aVaList );
    gGeneralLogger.trace( strFormat( aFormat, aVaList ).c_str() );
}

/**
 * warning
 * @param aFormat
 * @param ...
 */
void utl_warning
    (
    const char* aFormat, 
    ...
    )
{
    va_list aVaList;
    va_start( aVaList, aFormat );
    va_end( aVaList );
    gGeneralLogger.warning( strFormat( aFormat, aVaList ).c_str() );
}

/**
 * error
 * @param aFormat
 * @param ...
 */
void utl_error
    (
    const char* aFormat, 
    ...
    )
{
    va_list aVaList;
    va_start( aVaList, aFormat );
    va_end( aVaList );
    gGeneralLogger.error( strFormat( aFormat, aVaList ).c_str() );
}

namespace
{
    /**
     * Remove old logs
     */
    void removeOldLogs()
    {
        namespace bf = boost::filesystem;
        namespace bp = boost::posix_time;

        bf::path logDir( utl::Loggable::getLogDir() );
        if( !bf::exists( logDir ) )
        {
            return; // the log dir not exists
        }

        if( !bf::is_directory( logDir ) )
        {
            return; // logDir is not a dir
        }

        bf::directory_iterator dirIter( logDir );
        bf::directory_iterator endIter;
        bp::ptime now( bp::second_clock::universal_time() );
        for( ; dirIter != endIter ; ++dirIter )
        {
            if( !bf::is_regular_file( dirIter->status() ) )
            {
                continue;
            }

            bp::ptime lastWriteTime( bp::from_time_t( bf::last_write_time( *dirIter ) ) );
            if( lastWriteTime < now - bp::time_duration( bp::hours( LOG_FILE_KEEP_HOURS ) ) )
            {
                bf::remove( *dirIter );
            }
        }

    }
}

