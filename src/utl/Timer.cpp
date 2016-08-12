#include "Timer.h"

namespace
{
    boost::mutex gDefaultTimerMutex;
}

namespace utl
{
    boost::shared_ptr<Timer> Timer::sDefaultTimer;

    /**
     * Ctor
     */
    Timer::Timer()
        : iTerminated( false )
        , iMainLoopThread( boost::bind( &Timer::mainLoop, this ) )
    {
    }

    /**
     * Dtor
     */
    Timer::~Timer()
    {
        iTerminated = true;
        iExpireCondition.notify_one();
        iMainLoopThread.join();
    }

    /**
      * Get default timer. Intentionally omit the mutex protection
      */
    boost::shared_ptr<Timer> Timer::getDefaultTimer()
    {
        boost::mutex::scoped_lock lock( gDefaultTimerMutex );
        if( NULL == sDefaultTimer )
        {
            sDefaultTimer.reset( new Timer() );
        }
        return sDefaultTimer;
    }

    /**
     * The static version connect
     * @param aTimedEventCallback
     * @param aReltiveTimeout
     * @param aTargetTimer
     * @return 
     */
    boost::signals::connection Timer::connectTo
        (
        const TimedEventCallback::slot_type& aTimedEventCallback,
        unsigned int aReltiveTimeout, // ms
        Timer* aTargetTimer
        )
    {
        if( NULL == aTargetTimer )
        {
            return getDefaultTimer()->connect( aTimedEventCallback, aReltiveTimeout );
        }
        else
        {
            return aTargetTimer->connect( aTimedEventCallback, aReltiveTimeout );
        }
    }
  
    /**
     * The member function version connect
     */
    boost::signals::connection Timer::connect
        (
        const TimedEventCallback::slot_type& aTimedEventCallback,
        unsigned int aReltiveTimeout // ms
        )
    {
        boost::system_time newDeadline = boost::get_system_time() + 
                                         boost::posix_time::milliseconds( aReltiveTimeout );
        
        boost::mutex::scoped_lock lock( iMutex );
        
        if( iTimedEventQueue.empty() || 
            newDeadline < iTimedEventQueue.top()->iDeadline )
        {
            iExpireCondition.notify_one(); // adjust the wait deadline
        }

        boost::shared_ptr<TimedEventEntry> eventEntry( new TimedEventEntry() );
        eventEntry->iDeadline = newDeadline;
        eventEntry->iRelTimeout = aReltiveTimeout;
        boost::signals::connection con = eventEntry->iCallback.connect( aTimedEventCallback );
        iTimedEventQueue.push( eventEntry );

        return con;
    }

    /**
     * The main loop of Timer
     */
    void Timer::mainLoop()
    {
        while( !iTerminated )
        {
            boost::mutex::scoped_lock lock( iMutex );
            if( iTimedEventQueue.empty() )
            {
                iExpireCondition.wait( lock );
            }
            else if( boost::get_system_time() < iTimedEventQueue.top()->iDeadline )
            {
                iExpireCondition.timed_wait( lock, iTimedEventQueue.top()->iDeadline );
            }

            boost::system_time now = boost::get_system_time();
            while( !iTerminated && 
                   !iTimedEventQueue.empty() &&
                   iTimedEventQueue.top()->iDeadline <= now )
            {
                boost::shared_ptr<TimedEventEntry> eventEntry = iTimedEventQueue.top();
                iTimedEventQueue.pop();
                if( !eventEntry->iCallback.empty() && eventEntry->iCallback() )
                {
                    eventEntry->iDeadline = now + boost::posix_time::milliseconds( eventEntry->iRelTimeout );
                    iTimedEventQueue.push( eventEntry );
                }
            }
            
        }
    }

    /**
     * Timed TimedEventGreater operator()
     */
    bool Timer::TimedEventGreater::operator()
        (
        boost::shared_ptr<utl::Timer::TimedEventEntry> aLhs,
        boost::shared_ptr<utl::Timer::TimedEventEntry> aRhs
        )
    {
        return aLhs->iDeadline > aRhs->iDeadline;
    }

}

