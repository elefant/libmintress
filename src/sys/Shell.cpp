#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <fstream>
#include <string.h>
#include <fcntl.h>
#include "Shell.h"
#include "utl/String.h"

namespace sys
{
    //
    // Private section
    //
    namespace
    {
        //
        // Type
        //
        class StaticInitializer
        {
        public:
            StaticInitializer();
            
            ~StaticInitializer();
        };

        typedef std::map<pid_t, shell::OnProcessTerminationCallback> RunningList;

        // 
        // Function
        //
        void cleanZombie();

        void sigchldHandler( int aSignal );

        //
        // Varaibles
        //
        RunningList gRunningList;
        
        pid_t gZombieList[ 4096 ];
        
        volatile unsigned int gZombieCount = 0;
        
        boost::mutex gMutex;

        StaticInitializer gStaticInitialier;
    }
    
    /**
     * Launch command
     * @param command
     * @return the process id 
     */
    pid_t shell::launch
        (
        const std::string& aCommand,
        const std::string& aStderrFile,
        const OnProcessTerminationCallback& aTerminationCallback
        )
    {
    	pid_t ret;
        
        std::vector<std::string> argv;
        if( !utl::strToArgv( aCommand, argv ) )
        {
            return 0;
        }

    	cleanZombie();

    	if( 0 == ( ret = fork() ) )
    	{
    		for( int i = 3 ; i < sysconf( _SC_OPEN_MAX ) ; i++ )
            {
    			::close( i );
            }

            //
            // Redirect stderr to aStderrFile
            //      
            if( !aStderrFile.empty() )
            {
                std::ofstream logFile( aStderrFile.c_str() );
                if( logFile )
                {
                    // Print original command
                    logFile << "Command:" << std::endl << std::endl;
                    logFile << aCommand << std::endl << std::endl;
                    
                    // Print argv
                    logFile << "argv:" << std::endl << std::endl;
                    for( size_t i = 0; i < argv.size(); i++ )
                    {
                        logFile << argv[i] << "|";
                    }
                    logFile << std::endl << std::endl;
                    
                    // stderr
                    logFile << "stderr:" << std::endl << std::endl;
                }
                logFile.close();

                int fd = open( aStderrFile.c_str(), O_WRONLY | O_APPEND, 0666 );
                if( fd > 0 )
                {
                    // Redirect stderr to aStderrFile
                    dup2( fd, STDERR_FILENO );
                    close( fd );
                }
                else
                {
                    dup2( open( "/dev/null", 0, 0 ), STDERR_FILENO );
                    close( fd );
                }
            }
            
            char** cargv = new char*[argv.size() + 1];
            for( size_t i = 0; i < argv.size(); i++ )
            {
                cargv[i] = new char[argv[i].length() + 1];
                strcpy( cargv[i], argv[i].c_str() );
            }
            cargv[argv.size()] = NULL;
            
    		(void)execvp( cargv[0], cargv );
            
            for( size_t i = 0; i < argv.size() + 1; i++ )
            {
                delete cargv[i];
            }
            delete cargv;

    		// execv never returns on success
    		exit( 127 );
    	}
    	else if( -1 != ret )
    	{
            boost::mutex::scoped_lock lock( gMutex );
    		cleanZombie();
    		gRunningList[ret] = aTerminationCallback;
    	}

    	return ( ret > 0 ? ret : 0 );
    }

    /**
     * Terminate some process
     * @param aPid
     */
    void shell::terminate
        (
        pid_t aPid 
        )
    {
    	boost::mutex::scoped_lock lock( gMutex );

    	cleanZombie();
        
    	if( gRunningList.erase( aPid ) )
        {
    		kill( aPid, SIGKILL );
        }
    }

    /**
     * Terminate all
     */
    void shell::terminateAll()
    {
    	RunningList::iterator it;

    	boost::mutex::scoped_lock lock( gMutex );
        
    	cleanZombie();
    	for( it = gRunningList.begin() ; it != gRunningList.end() ; ++it )
        {
    		kill( it->first, SIGKILL );
        }
    }

    /**
     * Is process running
     * @param aPid
     * @return 
     */
    bool shell::isRunning
        (
        pid_t aPid 
        )
    {
    	boost::mutex::scoped_lock lock( gMutex );
    	cleanZombie();
    	return ( gRunningList.find( aPid ) != gRunningList.end() );
    }

    /**
     * Get process id list
     * @return 
     */
    shell::PidList shell::getProcessList()
    {
    	boost::mutex::scoped_lock lock( gMutex );
    	
        cleanZombie();

        PidList list;
        RunningList::iterator it;
    	for( it = gRunningList.begin() ; it != gRunningList.end() ; ++it )
        {
    		list.push_back( it->first );
        }
        
    	return list;
    }

    //
    // Private implementation
    //
    namespace 
    {   
        /**
         * The initialization  
         */
        StaticInitializer::StaticInitializer() 
        { 
            struct sigaction sigAction;
            memset( &sigAction, 0, sizeof( struct sigaction ) );
            sigAction.sa_handler = sigchldHandler;
            sigAction.sa_flags = 0;

            sigaction( SIGCHLD, &sigAction, NULL );
        }

        /**
         * The de-initialization
         */
        StaticInitializer::~StaticInitializer() 
        { 
            shell::terminateAll();
        }

        /**
         * Clean all zombie process
         */
        void cleanZombie()
        {
            for( ; gZombieCount ; gZombieCount-- )
            {
    	        gRunningList.erase( gZombieList[ gZombieCount - 1 ] );
            }
        }

        /**
         * Detect any zombie process
         * @param 
         */
        void sigchldHandler( int )
        {
            pid_t deadChildPid;
            int exitCode;
            while( gZombieCount < ( sizeof( gZombieList ) / sizeof( gZombieList[0] ) ) )
        	{
        		switch( ( deadChildPid = waitpid( -1, &exitCode, WNOHANG ) ) )
        		{
    			case 0:
    			case -1:
    				return;

    			default:
                    {
                        boost::mutex::scoped_lock lock( gMutex );
                        sys::shell::OnProcessTerminationCallback callback;
                        RunningList::iterator i = gRunningList.find( deadChildPid );
                        if( i != gRunningList.end() )
                        {
                            callback = i->second;
                        }
                        lock.unlock();
                        
                        gZombieList[ gZombieCount ] = deadChildPid;
                        gZombieCount++;

                        if( !callback.empty() )
                        {
                            callback( deadChildPid, exitCode );
                        }
                    }
    				break;
        		}
        	}
        }

    } // end of unamed namespace
    
} // end of namespace sys

