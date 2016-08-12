#ifndef UTL_TIMER_H
#define UTL_TIMER_H

#include <boost/signal.hpp>
#include <queue>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace utl
{
    class Timer
    {
    public:
        typedef boost::signal<bool()> TimedEventCallback;

    private:
        struct TimedEventEntry
        {
            boost::system_time iDeadline;

            unsigned int iRelTimeout;

            TimedEventCallback iCallback;
        };

        struct TimedEventGreater
        {
            bool operator()
                (
                boost::shared_ptr<utl::Timer::TimedEventEntry> aLhs,
                boost::shared_ptr<utl::Timer::TimedEventEntry> aRhs
                );            
        };

        typedef std::priority_queue
            <
            boost::shared_ptr<TimedEventEntry>,
            std::deque<boost::shared_ptr<TimedEventEntry> >,
            TimedEventGreater
            > TimedEventQueue;
        
    public:    
        Timer();

        ~Timer();
        
        static boost::shared_ptr<Timer> getDefaultTimer();
        
        static boost::signals::connection connectTo
            (
            const TimedEventCallback::slot_type& aTimedEventCallback,
            unsigned int aReltiveTimeout, // ms
            Timer* aTargetTimer = NULL // default timer
            );
      
        boost::signals::connection connect
            (
            const TimedEventCallback::slot_type& aTimedEventCallback,
            unsigned int aReltiveTimeout
            );

        void mainLoop();

    private:
        bool iTerminated;
        
        TimedEventQueue iTimedEventQueue;

        boost::condition_variable iExpireCondition;

        boost::mutex iMutex;

        boost::thread iMainLoopThread;

        static boost::shared_ptr<Timer> sDefaultTimer;
    };
}

#endif

