
#pragma once

#include "abstractAsyncProvider.h"
#include "common/msgqueue.h"
#include "common/mtcounter.h"

namespace simpleServer {



class AsyncProviderImpl: public IAsyncProvider, public RefCntObj {
public:

	virtual void runAsync(const AsyncResource &resource, int timeout, const CompletionFn &complfn) override;

	virtual void runAsync(const CompletionFn &completion) override;


	virtual bool serve();

	virtual void releaseThreads();


	AsyncProviderImpl();



public:

	void setCountOfListeners(unsigned int count);

	void setCountOfThreads(unsigned int count);

	void stop();

	~AsyncProviderImpl();


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


class AsyncProvider: public RefCntPtr<AsyncProviderImpl> {
public:

	using RefCntPtr<AsyncProviderImpl>::RefCntPtr;

	static AsyncProvider create(unsigned int numThreads=1, unsigned int numListeners=1);
	void setCountOfListeners(unsigned int count);
	void setCountOfThreads(unsigned int count);
	void stop();

	~AsyncProvider();


};


}
