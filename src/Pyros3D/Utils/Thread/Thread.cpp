////============================================================================
//// Name        : Thread.cpp
//// Author      : Duarte Peixinho
//// Version     :
//// Copyright   : ;)
//// Description : Thread
////============================================================================

#include <map>
#include <Pyros3D/Utils/Thread/Thread.h>

namespace p3d {
    
    std::map<uint32, _Thread> Thread::_ThreadsList;
    uint32 Thread::ThreadsCounter = 0;
    
    uint32 Thread::AddThread(void* (*ThreadFunction)(void*), void* arg)
    {
        ThreadsCounter++;
        // Create Thread
        uint32 pID = pthread_create( &_ThreadsList[ThreadsCounter].thread, NULL, *ThreadFunction, arg);
        if (pID!=0) echo("ERROR: Thread Not Registered, CODE: " + NumberToString(pID));
        return ThreadsCounter;
    }
    
    bool Thread::RemoveThread(const uint32 ThreadID)
    {
        // End Thread
        uint32 pID = pthread_join(_ThreadsList[ThreadID].thread,NULL);
        if (pID == 0)
        {
            _ThreadsList.erase(ThreadID);
            if (_ThreadsList.size()==0) ThreadsCounter = 0;
            return true;
        }
        echo("ERROR: Thread Not Terminated, CODE: " + NumberToString(pID));
        return false;
    }
    void Thread::LockThread(const uint32 ThreadID)
    {
        // Lock Mutex
        uint32 pID = pthread_mutex_lock (&_ThreadsList[ThreadID].mutex);
        if (pID!=0) echo("ERROR: Mutex Not Locked, CODE: " + NumberToString(pID));
    }
    void Thread::UnlockThread(const uint32 ThreadID)
    {
        // Unlock Mutex
        uint32 pID = pthread_mutex_unlock(&_ThreadsList[ThreadID].mutex);
        if (pID!=0) echo("ERROR: Mutex Not Unlocked, CODE: " + NumberToString(pID));
    }
};