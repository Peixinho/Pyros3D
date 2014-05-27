//============================================================================
// Name        : Thread.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Thread
//============================================================================

#ifndef THREAD_H
#define THREAD_H

#include "../../Core/Math/Math.h"
#include "../../Core/Logs/Log.h"
#include "../../Other/Export.h"
#include <pthread.h>
#include <map>

namespace p3d {
    
    struct _Thread {
        pthread_t thread;
        pthread_mutex_t mutex;
    };
    
    class PYROS3D_API Thread {
    public:
        
        // Add Thread
        static uint32 AddThread(void* (*ThreadFunction)(void*), void* arg);
        // Remove Thread
        static bool RemoveThread(const uint32 &ThreadID);
        // Lock & Unlock
        static void LockThread(const uint32 &ThreadID);
        static void UnlockThread(const uint32 &ThreadID);
        
        static uint32 GetActiveThreads() { return _ThreadsList.size(); }
    protected:
        
        // Threads List
        static std::map<uint32, _Thread> _ThreadsList;
        static uint32 ThreadsCounter;
        
    };
    
};

#endif  /* THREAD_H */