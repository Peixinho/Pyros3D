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

	Thread::~Thread()
	{
		// std::thread::~thread() calls std::terminate() if the thread is
		// still joinable - unlike the old pthread_t member (a POD handle;
		// destroying a Thread with a still-running pthread just leaked
		// the handle, it never crashed). Nothing in this codebase relies
		// on outliving an un-joined Thread today, but detach() here keeps
		// that same "leaks rather than crashes" behavior for whoever
		// constructs one without pairing Launch() with Terminate()+
		// CheckThreads() first.
		if (thread.joinable())
			thread.detach();
	}

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
		// std::thread's constructor throws std::system_error on failure
		// instead of returning an error code (pthread_create's model) -
		// caught here so a launch failure logs and leaves this Thread
		// untracked, exactly like the old pID != 0 branch, instead of
		// taking down the whole process via an uncaught exception.
		try
		{
			thread = std::thread(__method, __arg);
			Thread::threads.push_back(this);
		}
		catch (const std::system_error &e)
		{
			echo("ERROR: Thread Not Registered, CODE: " + NumberToString(e.code().value()));
		}
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
					// join() throws std::system_error instead of
					// pthread_join's return-code model - same catch/log
					// shape as Launch() above, and the entry is still
					// erased either way (matches the old code's
					// unconditional erase regardless of the join result).
					try
					{
						if ((*i)->thread.joinable())
							(*i)->thread.join();
					}
					catch (const std::system_error &e)
					{
						echo("ERROR: Thread Not Terminated, CODE: " + NumberToString(e.code().value()));
					}
					i = Thread::threads.erase(i);
				}
				else i++;
			}
		}
	}

	void Thread::CreateMutex() {}
	void Thread::LockMutex()
	{
		mutex.lock();
		locked = true;
	}
	void Thread::UnlockMutex()
	{
		mutex.unlock();
		locked = false;
	}
	void Thread::TerminateMutex() {}
};