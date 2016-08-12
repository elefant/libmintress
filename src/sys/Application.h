#ifndef SYS_APPLICATION_H
#define SYS_APPLICATION_H

#include <string>
#include <boost/function.hpp>

namespace sys
{
    namespace application
    {
        typedef boost::function<void( const std::string& )> OutputCallback;
        
        typedef boost::function<void( const std::string& )> SegFaultHandler;
        
        typedef boost::function<void( const std::string& )> KilledHandler;
        
        std::string getExeName();

        std::string getExePath();
        
        std::string getExeDir();
        
        void enableSegFaultBacktrace
            (
            const OutputCallback& aOutput = OutputCallback()
            );
        
        void setSegFaultHandler
            (
            const SegFaultHandler& aHandler
            );
        
        void setKilledHandler
            (
            const KilledHandler& aHandler
            );
    }
}

#endif

