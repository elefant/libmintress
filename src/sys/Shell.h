#ifndef SYS_SHELL_H
#define SYS_SHELL_H

#include <unistd.h>
#include <vector>
#include <set>
#include <string>
#include <boost/thread/mutex.hpp>
#include <boost/function.hpp>

namespace sys
{
    namespace shell
    {
        typedef boost::function<void( int, int )> OnProcessTerminationCallback;
    
        typedef std::vector<pid_t> PidList;

        pid_t launch
            (
            const std::string& aCommand,
            const std::string& aStderrFile = "",
            const OnProcessTerminationCallback& aTerminationCallback = OnProcessTerminationCallback()
            );

        void terminate
            (
            pid_t aPid 
            );

        void terminateAll();

        PidList getProcessList();

        bool isRunning
            (
            pid_t aPid 
            );
    };
}

#endif /* __SHELL_HPP__ */

