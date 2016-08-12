#ifndef SYS_THREAD_POOL_H
#define SYS_THREAD_POOL_H

#include <boost/thread.hpp>
#include <boost/function.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/condition.hpp>
#include <vector>
#include <list>
#include <string>
#include "utl/Logger.h"

namespace sys
{
    class ThreadPool : private utl::Logger
    {    
    public:
        struct Task;
        
        typedef boost::shared_ptr<boost::thread> ThreadPtr;
        typedef boost::function<void()> EntryPoint;
        typedef boost::function<void( int, const Task& )> TaskDoneCallback;

        struct Task
        {
            EntryPoint iEntryPoint;
            TaskDoneCallback iOnTaskDone;
            std::string iName;
        };
        
    public:
        ThreadPool
            (
            int aInitialSize,
            int aCapacity
            );

        ~ThreadPool();

        bool postTask
            (
            EntryPoint aEntryPoint,
            const std::string& aTaskName = std::string()
            );

        void joinAll();

    private:
        void doWork
            (
            int aId
            );

        ThreadPtr spawnWorkerThread
            (
            int aId
            );

        void handleTaskDone
            (
            int aId,
            const Task& aDoneTask
            );

    private:
        std::vector<ThreadPtr> iWorkerList;

        size_t iCapacity;

        boost::mutex iMutex;    

        bool iTerminated;

        std::list<Task> iPendingTaskList;

        boost::condition_variable iWaitForTaskCondition;

        size_t iWorkingCount;
    };
}

#endif
