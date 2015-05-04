//============================================================================
// Name        : Thread.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Thread
//============================================================================

#ifndef THREAD_H
#define THREAD_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <Pyros3D/Other/Export.h>
#include <pthread.h>
#include <vector>

namespace p3d {
    
    class PYROS3D_API Thread {

        public:

            Thread(void* (*ThreadFunction)(void*));
            Thread(void* (*ThreadFunction)(void*), void* arg);
            virtual ~Thread();
            void Launch();
            void Terminate();
            void CreateMutex();
            void LockMutex();
            void UnlockMutex();
            void TerminateMutex();
            bool IsFinished() { return finished; }
            bool IsLocked() { return locked; }

            // Remove Thread
            static void CheckThreads();
            
            static uint32 GetActiveThreads() { return ThreadsCounter; }

        protected:

            pthread_t thread;
            pthread_mutex_t mutex;
            bool finished;
            bool locked;

            void* (*__method)(void*);
            void* __arg;

            // Threads List
            static uint32 ThreadsCounter;
            static std::vector<Thread*> threads;
    };
    
};

#endif  /* THREAD_H */