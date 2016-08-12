#include "ThreadPool.h"

namespace sys
{       
    /**
     * Ctor
     * @param aInitialCapacity
     * @param aCapacity
     */
    ThreadPool::ThreadPool
        (
        int aInitialCapacity,
        int aCapacity
        )
        : utl::Logger( "ThreadPool", utl::Logger::ENABLE_NONE )
        , iCapacity( aCapacity )
        , iTerminated( false )
        , iWorkingCount( 0 )
    {
        boost::mutex::scoped_lock lock( iMutex );
        
        for( int i = 0; i < aInitialCapacity; i++ )
        {
            iWorkerList.push_back( spawnWorkerThread( i ) );
        }
    }

    /**
     * Dtor
     */
    ThreadPool::~ThreadPool()
    {
        boost::mutex::scoped_lock lock( iMutex );
        iTerminated = true;
        lock.unlock();    
        iWaitForTaskCondition.notify_all();
        joinAll();
    }

    /**
     * Join all threads
     */
    void ThreadPool::joinAll()
    {
        for( size_t i = 0; i < iWorkerList.size(); i++ )
        {
            trace( "[%d] join", i );
            iWorkerList[i]->join();
        }
    }

    /*
    * Spawn a worker thread
    */
    ThreadPool::ThreadPtr ThreadPool::spawnWorkerThread
        (
        int aId
        )
    {
        return ThreadPtr
            (
            new boost::thread( boost::bind( &ThreadPool::doWork, this, aId ) )
            );
    }

    /**
     * Post a task to the pending task list but not handle it right away
     * @param aEntryPoint
     * @return 
     */
    bool ThreadPool::postTask
        (
        EntryPoint aEntryPoint,
        const std::string& aTaskName
        )
    {
        if( iTerminated )
        {
            return false;
        }
    
        Task task;
        task.iEntryPoint = aEntryPoint;
        task.iOnTaskDone = boost::bind( &ThreadPool::handleTaskDone, this, _1, _2 );
        task.iName = aTaskName;
        
        boost::mutex::scoped_lock lock( iMutex );
        
        if( iWorkingCount == iWorkerList.size() && 
            iWorkingCount < iCapacity )
        {
            iWorkerList.push_back( spawnWorkerThread( iWorkingCount ) );
        }
        
        iPendingTaskList.push_back( task );
        
        lock.unlock();

        iWaitForTaskCondition.notify_all();

        return true;
    }

    /**
     * Handle task done. for debug
     * @param aTaskDoneThread
     */
    void ThreadPool::handleTaskDone
        (
        int aId,
        const Task& aDoneTask
        )  
    {
        trace( "Worker[%d] has done task:%s", aId, aDoneTask.iName.c_str() );
    }
 
    /**
     * The entry point of the worker thread
     */
    void ThreadPool::doWork
        (
        int aId
        )
    {
        trace( "[%d] is spawned!", aId );
        
        while( 1 )
        {
            boost::mutex::scoped_lock lock( iMutex ); 

            if( iTerminated )
            {   trace( "[%d] We are going to die", aId );
                break;
            }
            
            if( iPendingTaskList.empty() )
            {
                trace( "[%d] No task to do. Wait...", aId );
                
                iWaitForTaskCondition.wait( lock );
 
                if( iTerminated )
                {
                    trace( "[%d] Notified to die", aId );
                    if( !iPendingTaskList.empty() )
                    {
                        trace( "[%d] Got task but terminated", aId );
                    }
                    break;
                }

                if( iPendingTaskList.empty() )
                {        
                    continue;
                }
            }
        
            trace( "[%d] Got %d tasks to do!", aId, iPendingTaskList.size() );
            
            Task task = iPendingTaskList.front();
            iPendingTaskList.pop_front();
            
            iWorkingCount++;
            
            lock.unlock();

            trace( "[%d] Start working: %s!", aId, task.iName.c_str() );

            task.iEntryPoint();
            task.iOnTaskDone( aId, task );
            
            trace( "[%d] Finish working: %s", aId, task.iName.c_str() );
            
            lock.lock();
            iWorkingCount--;
            lock.unlock();
        }

        trace( "[%d] doWork end...", aId );
    }    
}

