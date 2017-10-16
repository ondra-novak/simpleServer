
#pragma once

#include "asyncProvider.h"
#include "common/msgqueue.h"
#include "common/mtcounter.h"

namespace simpleServer {



class ThreadPoolAsyncImpl: public AbstractAsyncProvider {
public:

	virtual void runAsync(const AsyncResource &resource, int timeout, const CompletionFn &complfn) override;

	virtual void runAsync(const CompletionFn &completion) override;


	virtual void stop() override;


	ThreadPoolAsyncImpl();



public:

	void setCountOfListeners(unsigned int count);

	void setCountOfThreads(unsigned int count);


	~ThreadPoolAsyncImpl();


protected:
	MsgQueue<PEventListener> tQueue;
	unsigned int reqListenerCount = 1;
	unsigned int reqThreadCount = 1;
	MTCounter threadCount;
	std::queue<PEventListener> cQueue;
	std::mutex lock;
	typedef std::lock_guard<std::mutex> Sync;


	PEventListener getListener();

	void worker();


};


class ThreadPoolAsync: public RefCntPtr<ThreadPoolAsyncImpl> {
public:

	using RefCntPtr<ThreadPoolAsyncImpl>::RefCntPtr;

	static AsyncProvider create(unsigned int numThreads=1, unsigned int numListeners=1);
	void setCountOfListeners(unsigned int count);
	void setCountOfThreads(unsigned int count);
	void stop();

	~ThreadPoolAsync();

	operator AsyncProvider() const;


};


}
