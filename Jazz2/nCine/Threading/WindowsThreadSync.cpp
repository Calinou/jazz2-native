#include "ThreadSync.h"

namespace nCine {

	///////////////////////////////////////////////////////////
	// Mutex CLASS
	///////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////
	// CONSTRUCTORS and DESTRUCTOR
	///////////////////////////////////////////////////////////

	Mutex::Mutex()
	{
		::InitializeCriticalSection(&handle_);
	}

	Mutex::~Mutex()
	{
		::DeleteCriticalSection(&handle_);
	}

	///////////////////////////////////////////////////////////
	// PUBLIC FUNCTIONS
	///////////////////////////////////////////////////////////

	void Mutex::lock()
	{
		::EnterCriticalSection(&handle_);
	}

	void Mutex::unlock()
	{
		::LeaveCriticalSection(&handle_);
	}

	int Mutex::tryLock()
	{
		return ::TryEnterCriticalSection(&handle_);
	}

	///////////////////////////////////////////////////////////
	// CondVariable CLASS
	///////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////
	// CONSTRUCTORS and DESTRUCTOR
	///////////////////////////////////////////////////////////

	CondVariable::CondVariable()
		: waitersCount_(0)
	{
		events_[0] = ::CreateEvent(nullptr, FALSE, FALSE, nullptr); // Signal
		events_[1] = ::CreateEvent(nullptr, TRUE, FALSE, nullptr); // Broadcast
		::InitializeCriticalSection(&waitersCountLock_);
	}

	CondVariable::~CondVariable()
	{
		::CloseHandle(events_[0]); // Signal
		::CloseHandle(events_[1]); // Broadcast
		::DeleteCriticalSection(&waitersCountLock_);
	}

	///////////////////////////////////////////////////////////
	// PUBLIC FUNCTIONS
	///////////////////////////////////////////////////////////

	void CondVariable::wait(Mutex& mutex)
	{
		::EnterCriticalSection(&waitersCountLock_);
		waitersCount_++;
		::LeaveCriticalSection(&waitersCountLock_);

		mutex.unlock();
		waitEvents();
		mutex.lock();
	}

	void CondVariable::signal()
	{
		::EnterCriticalSection(&waitersCountLock_);
		const bool haveWaiters = (waitersCount_ > 0);
		::LeaveCriticalSection(&waitersCountLock_);

		if (haveWaiters)
			::SetEvent(events_[0]); // Signal
	}

	void CondVariable::broadcast()
	{
		::EnterCriticalSection(&waitersCountLock_);
		const bool haveWaiters = (waitersCount_ > 0);
		::LeaveCriticalSection(&waitersCountLock_);

		if (haveWaiters)
			::SetEvent(events_[1]); // Broadcast
	}

	///////////////////////////////////////////////////////////
	// PRIVATE FUNCTIONS
	///////////////////////////////////////////////////////////

	void CondVariable::waitEvents()
	{
		const int result = ::WaitForMultipleObjects(2, events_, FALSE, INFINITE);

		::EnterCriticalSection(&waitersCountLock_);
		waitersCount_--;
		const bool isLastWaiter = (result == (WAIT_OBJECT_0 + 1)) && (waitersCount_ == 0);
		::LeaveCriticalSection(&waitersCountLock_);

		if (isLastWaiter)
			::ResetEvent(events_[1]); // Broadcast
	}

}
