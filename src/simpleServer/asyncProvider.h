#pragma once

#include <functional>

#include "refcnt.h"

#include "stringview.h"


namespace simpleServer {


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
	asyncError
};


class IAsyncProvider {
public:

	typedef std::function<void(AsyncState)> CompletionFn;


	virtual void runAsync(const AsyncResource &resource, int timeout, const CompletionFn &complfn) = 0;

	virtual void runAsync(const CompletionFn &completion) = 0;


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
		Fn ccpy(completion);
		ptr->runAsync([ccpy](AsyncState){ccpy();});
	}

	void stop() const {
		ptr->stop();
	}

};

class AbstractStreamEventDispatcher: public AbstractAsyncProvider {
public:

	typedef std::function<void()> Task;

	virtual Task waitForEvent() = 0;


	///returns true, if the listener doesn't contain any asynchronous task
	virtual bool empty() const = 0;
	///Move all asynchronous tasks to different listener
	/** @param target target provider
	 *
	 *  @note function also calls stop(). This is the only way how to destroy
	 *  dispatcher without interrupting async. operation
	 */
	virtual void moveTo(AsyncProvider target) = 0;

	///Returns count of pending asynchronous tasks
	/**
	 * @return count of pending tasks
	 */
	virtual unsigned int getPendingCount() const = 0;

	static RefCntPtr<AbstractStreamEventDispatcher> create();
};

typedef RefCntPtr<AbstractStreamEventDispatcher> PStreamEventDispatcher;


}
