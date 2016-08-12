#include "utl/Misc.h"

#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/thread_time.hpp>

namespace utl
{
    /**
     * Wait for condition
     * @param aExitCondition
     * @param aTimeout
     * @param aPollingPeriodMs
     * @return 
     */
    bool waitUntil
        (
        boost::function<bool()> aExitCondition,
        int aTimeout,
        int aPollingPeriodMs
        )
    {
        boost::system_time absTimeout;
        if( aTimeout > 0 )
        {
            const boost::posix_time::milliseconds REL_TIMEOUT( aTimeout );
            absTimeout = boost::get_system_time() + REL_TIMEOUT;
        }
 
        while( !aExitCondition() )
        {
            if( aTimeout > 0 && boost::get_system_time() > absTimeout )
            {
                return false;
            }
            
            usleep( aPollingPeriodMs * 1000 );
        }
        
        return true;
    }
}
