////============================================================================
//// Name        : Thread.cpp
//// Author      : Duarte Peixinho
//// Version     :
//// Copyright   : ;)
//// Description : Thread
////============================================================================

#include <Pyros3D/Utils/Thread/Thread.h>

namespace p3d {

	std::vector<Thread*> Thread::threads;
	uint32 Thread::ThreadsCounter = 0;

	Thread::~Thread() {}

	Thread::Thread(void* (*ThreadFunction)(void*))
	{
		__method = ThreadFunction;
		__arg = NULL;
	}

	Thread::Thread(void* (*ThreadFunction)(void*), void* arg)
	{
		__method = ThreadFunction;
		__arg = arg;
	}

	void Thread::Launch()
	{
		finished = false;
		uint32 pID = pthread_create(&thread, NULL, *__method, __arg);
		if (pID != 0) echo("ERROR: Thread Not Registered, CODE: " + NumberToString(pID));
		else Thread::threads.push_back(this);
	}

	void Thread::Terminate()
	{
		finished = true;
	}

	void Thread::CheckThreads()
	{
		if (Thread::threads.size() > 0)
		{
			std::vector<Thread*>::iterator i = Thread::threads.begin();
			while (i != Thread::threads.end())
			{
				if ((*i)->finished)
				{
					uint32 pID = pthread_join((*i)->thread, NULL);
					i = Thread::threads.erase(i);
					if (pID != 0)
						echo("ERROR: Thread Not Terminated, CODE: " + NumberToString(pID));
				}
				else i++;
			}
		}
	}

	void Thread::CreateMutex()
	{
		pthread_mutex_init(&mutex, NULL);
	}
	void Thread::LockMutex()
	{
		pthread_mutex_lock(&mutex);
		locked = true;
	}
	void Thread::UnlockMutex()
	{
		pthread_mutex_unlock(&mutex);
		locked = false;
	}
	void Thread::TerminateMutex()
	{
		pthread_mutex_destroy(&mutex);
	}
};