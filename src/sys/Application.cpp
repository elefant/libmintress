#include "Application.h"
#include "utl/String.h"
#include <string.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef __USE_GNU
#define __USE_GNU
#endif

#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>
#include <unistd.h>

namespace 
{
    /* This structure mirrors the one found in /usr/include/asm/ucontext.h */
    typedef struct _sig_ucontext 
    {
        unsigned long     uc_flags;
        struct ucontext   *uc_link;
        stack_t           uc_stack;
        struct sigcontext uc_mcontext;
        sigset_t          uc_sigmask;
    } sig_ucontext_t;

    class StaticSignalRegister
    {
    public:
        StaticSignalRegister();
    };
    
    void crit_err_hdlr
        (
        int sig_num, 
        siginfo_t * info, 
        void * ucontext
        );
    
    sys::application::SegFaultHandler gSegFaultHandler;
    
    sys::application::KilledHandler gKilledHandler;
    
    StaticSignalRegister gStaticSignalRegister;
}

namespace sys
{
    /**
     * Get the executable name
     * @return 
     */
    std::string application::getExeName()
    {
        std::string exePath = getExePath();
        size_t offset = exePath.rfind( '/' );
        if( std::string::npos != offset && offset + 1 < exePath.length() )
        {
            return exePath.substr( offset + 1 );
        }
        else
        {
            return "";
        }
    }

    /**
     * Get the full executable path
     * @return 
     */
    std::string application::getExePath()
    {
        char buf[256] = { 0 };
        readlink( "/proc/self/exe", buf, sizeof( buf ) );
        return buf;
    }
    
    /**
     * Get execution directory 
     * @return 
     */
    std::string application::getExeDir()
    {
        std::string exePath = getExePath();
        size_t dirOffset = exePath.rfind( '/' );
        if( std::string::npos != dirOffset )
        {
            return exePath.substr( 0, dirOffset );
        }
        else
        {
            char cwd[PATH_MAX] = { 0 };
            getcwd( cwd, PATH_MAX );
            return cwd;
        }
    }
    
    /**
     * Set segmentation fault handler
     * @param aHandler
     */
    void application::setSegFaultHandler
        (
        const SegFaultHandler& aHandler
        )
    {
        gSegFaultHandler = aHandler;
    }
    
    /**
     * Set killed handler
     * @param aHandler
     */
    void application::setKilledHandler
        (
        const KilledHandler& aHandler
        )
    {
        gKilledHandler = aHandler;
    }
    
}

namespace
{
    /**
     * 
     * @param sig_num
     * @param info
     * @param ucontext
     */
    void crit_err_hdlr
        (
        int sig_num, 
        siginfo_t * info, 
        void * ucontext
        )
    {
        void *             array[50];
        void *             caller_address;
        char **            messages;
        int                size, i;
        sig_ucontext_t *   uc;

        uc = (sig_ucontext_t *)ucontext;

        /* Get the address at the time the signal was raised from the EIP (x86) */
        caller_address = (void *) uc->uc_mcontext.eip;   
        (void)uc->uc_flags;

        fprintf
            (
            stderr, 
            "signal %d (%s), address is %p from %p\n", 
            sig_num, 
            strsignal( sig_num ), 
            info->si_addr, 
            (void *)caller_address
            );

        size = backtrace(array, 50);

        /* overwrite sigaction with caller's address */
        array[1] = caller_address;

        messages = backtrace_symbols(array, size);

        std::string backtraceString;
        /* skip first stack frame (points here) */
        for( i = 1; i < size && messages != NULL; ++i )
        {
            backtraceString += utl::strFormat( "[bt]: (%d) %s\n", i, messages[i] );
        }

        free( messages );
        
        if( !gSegFaultHandler.empty() )
        {
            gSegFaultHandler( backtraceString );
        }
        else
        {
            fprintf( stderr, "%s\n", backtraceString.c_str() );
        }

        exit( EXIT_FAILURE );
    }
    
    /**
     * Exception handler
     */
    StaticSignalRegister::StaticSignalRegister()
    {
        //
        // Set segmentation fault signal handler 
        //
        struct sigaction sigact;

        sigact.sa_sigaction = crit_err_hdlr;
        sigact.sa_flags = SA_RESTART | SA_SIGINFO;

        if( sigaction( SIGSEGV, &sigact, (struct sigaction *)NULL ) != 0 )
        {
            fprintf
                (
                stderr, 
                "error setting signal handler for %d (%s)\n",
                SIGSEGV, 
                strsignal( SIGSEGV ) 
                );

            exit( EXIT_FAILURE );
        }
        
        //
        // Set kill signal handler
        //
        // TODO:
    }
}
