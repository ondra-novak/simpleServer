
#pragma once

#include "asyncProvider.h"
#include "shared/msgqueue.h"
#include "shared/countdown.h"
#include "shared/dispatcher.h"
#include "shared/defer.h"

namespace simpleServer {

using ondra_shared::MsgQueue;
using ondra_shared::Countdown;
using ondra_shared::Dispatcher;
using ondra_shared::DeferContext;



class ThreadPoolAsyncImpl: public AbstractAsyncProvider {
public:

	virtual void runAsync(const AsyncResource &resource, int timeout, CompletionFn &&complfn) override;

	virtual void runAsync(CustomFn &&completion) override;

	virtual void stop() override;

	virtual void cancel(const AsyncResource &resource) override;

public:

	void setCountOfDispatchers(unsigned int count);

	void setCountOfThreads(unsigned int count);

	void setTasksPerDispLimit(unsigned int count);

	~ThreadPoolAsyncImpl();


protected:

	Dispatcher dQueue;
//	MsgQueue<PStreamEventDispatcher> tQueue;
	unsigned int reqDispatcherCount = 1;
	unsigned int reqThreadCount = 1;
	unsigned int taskLimit = -1;
	Countdown threadCount;
	bool exitFlag = false;
	std::queue<PStreamEventDispatcher> cQueue;
	std::mutex lock;
	typedef std::lock_guard<std::mutex> Sync;


	PStreamEventDispatcher getListener();

	void worker() noexcept;
	class InvokeNetworkDispatcher;
	void waitForTask(const PStreamEventDispatcher &sed) noexcept;
	void onExitDispatcher(const PStreamEventDispatcher &sed) noexcept;

private:
	void checkThreadCount();
};


class ThreadPoolAsync: public RefCntPtr<ThreadPoolAsyncImpl> {
public:

	using RefCntPtr<ThreadPoolAsyncImpl>::RefCntPtr;

	///Creates thread pool for asynchronous operations
	/**
	 * Function can create one thread or thread pool depends on arguments
	 * @param numThreads desired count of threads.
	 * @param numDispatchers count of dispatchers. This allows to reduce overhead on
	 * single thread when there are a lot of pending asynchronous operations.
	 * @param tasksPerDispLimit specifies maximum count of pending tasks per dispatcher. Once
	 * there is no room in any dispatcher, additional dispatcher can be allocated, which can also
	 * require to add a new thread to the pool. Set -1 to disable this limit. Note that some
	 * implementations (on some platforms) can have hardcoded a limit.
	 *
	 *
	 * @note in case when all dispatchers are running out of limits, additional
	 * dispatchers can be silently added with appropriate count of threads.
	 * Never assume that single-threaded pool can alvays stay single-threaded.
	 */
	static AsyncProvider create(unsigned int numThreads=1, unsigned int numDispatchers=1, unsigned int tasksPerDispLimit=60);
	///Changes count of dispatchers
	/** @param count desired count of dispatchers
	 *
	 * @note changes are not applied immediately, dispatchers are removed as part
	 * of thread task, so there must be at least one idle thread which can handle
	 * this operation
	 */
	void setCountOfDispatchers(unsigned int count);
	///Changes count of threads
	/**
	 * @param count desired count of threads
	 *
	 * @note changes are noy applied immediately. New threads are created only if
	 * there are no idle threads. In case of decreasing count the thread are
	 * destroyed as they finish their work
	 */
	void setCountOfThreads(unsigned int count);


	void setTasksPerDispLimit(unsigned int count);

	///Stops the operation
	/** It is mandatory to call stop() explicitly before the last reference of the
	 * thread pool is release. This is because there reference count can be much
	 * higher then expected. Every pending asynchronous operation can allocate additional
	 * reference. This function cancels all pending operations which can help to
	 * reduce reference count, so the object leff clear before destuction. Function
	 * also exits all threads (and waits for their exiting)
	 */
	void stop();

	~ThreadPoolAsync();

	operator AsyncProvider() const;


};


}
