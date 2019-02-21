#pragma once

#include <functional>

#include "shared/refcnt.h"


namespace ondra_shared {
	class Worker;
}

namespace simpleServer {

using ondra_shared::RefCntObj;
using ondra_shared::RefCntPtr;
using ondra_shared::Worker;


class AsyncResource;

enum AsyncState {
	///asynchronous operation completted successfuly
	asyncOK,
	///asynchronous operation completted successfuly, result is EOF
	asyncEOF,
	///asynchronous operation timeouted
	asyncTimeout,
	///asynchronous operation failed with and error
	/**
	 * The error state is stored as current exeception. You can use std::current_exception to determine, which error happened
	 */
	asyncError,
	///indicates that asynchronous operation has been canceled
	asyncCancel
};


class IAsyncProvider {
public:

	///Declaration of completion function
	/**
	 * @param AsyncState reason of completion.
	 */
	typedef std::function<void(AsyncState) > CompletionFn;
	///Declaration of custom asynchronous function
	typedef std::function<void()> CustomFn;

	///runs asynchronous I/O operation
	/**
	 * @param resource Asynchronous resource. The object is platform depend. It identifies resource
	 * which will be used in asynchronous operation and operation itself. The most of Streams
	 * and other I/O functions uses this function internally.
	 * @param timeout specifies timeout in miliseconds
	 * @param complfn function called when I/O operation completes
	 */
	virtual void runAsync(const AsyncResource &resource, int timeout, const CompletionFn &complfn) = 0;

	///Runs function asynchronously
	/** Function calls directly complettion function but it is executed in completion thread, which
	 * runs paralel to current thread. If you call this function in completion thread, the operation
	 * may be postponed until there is at least one thread ready to run
	 * @param completion user function to run in completion thread
	 *
	 * You can use this function to execute paralel tasks. Note that massive paralel task
	 * will probably block other I/O completion functions
	 */
	virtual void runAsync(const CustomFn &completion) = 0;

	///Cancels waiting for asynchronous resource
	/**
	 * @param resource resource to cancel waiting
	 *
	 * @note canceled operation complettes with status asyncCancel. If there are multiple completion
	 * functions, all of them are called.
	 *
	 * @note function has linear complexity. It can also involve many threads as it need to search all
	 * dispatchers for specified resource.
	 *
	 */
	virtual void cancel(const AsyncResource &resource) = 0;

	///Stops the asynchronous provider
	/** cancels all waiting I/O operations and stops threads. You need to call this function if you
	 * need to stop all I/O operations before the object is destroyed
	 */

	virtual void stop() = 0;

	virtual ~IAsyncProvider() {}
};


class AbstractAsyncProvider: public RefCntObj, public IAsyncProvider{
public:


};


class AsyncProvider: public RefCntPtr<AbstractAsyncProvider> {
public:

	using RefCntPtr<AbstractAsyncProvider>::RefCntPtr;

	template<typename Fn>
	void runAsync(const AsyncResource &resource, int timeout, const Fn &completion) const {
		ptr->runAsync(resource, timeout,completion);
	}

	template<typename Fn>
	void runAsync(const Fn &completion) const {
		ptr->runAsync(completion);
	}

	void stop() const {
		ptr->stop();
	}

	void cancel(const AsyncResource &x) const {
		ptr->cancel(x);
	}

	template<typename Fn>
	void operator>>(const Fn &completion) const {
		ptr->runAsync(completion);
	}

	///Converts AsyncProvider to Worker
	operator Worker() const;

};

///Abstract class which helps to build custom async. providers
class AbstractStreamEventDispatcher: public AbstractAsyncProvider {
public:

	///Contains packed task, which must be executed in thread which providing execution of async operation
	class Task: public CompletionFn {
	public:
		Task() {}
		Task(CompletionFn fn, AsyncState state):CompletionFn(fn),state(state) {}
		void operator()() noexcept {
			if (*this != nullptr) CompletionFn::operator ()(state);
		}
	protected:
		AsyncState state;
	};

	///Thread providing asynchronous operation must call this function to obtain a task.
	/**
	 * Function block thread
	 * @return a task. Thread must check, whether task is null. In case the task is null (nullptr),
	 * then it should break the loop and finish execution
	 */
	virtual Task wait() = 0;


	///returns true, if the listener doesn't contain any asynchronous task
	virtual bool empty() const = 0;

	///Returns count of pending asynchronous tasks
	/**
	 * @return count of pending tasks
	 */
	virtual unsigned int getPendingCount() const = 0;


	///Creates platform depend StreamEventDispatcher for AsyncResource
	static RefCntPtr<AbstractStreamEventDispatcher> create();
};

typedef RefCntPtr<AbstractStreamEventDispatcher> PStreamEventDispatcher;


}
