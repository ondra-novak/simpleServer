
#pragma once

#include "abstractAsyncProvider.h"
#include "common/msgqueue.h"
#include "common/mtcounter.h"

namespace simpleServer {



class AsyncProvider: public IAsyncProvider, public RefCntObj {
protected:


/*
	virtual void receive(const AsyncResource &resource,
			MutableBinaryView buffer,
			int timeout,
			Callback completion);

	virtual void send(const AsyncResource &resource,
			BinaryView buffer,
			int timeout,
			Callback completion);
*/
	virtual void runAsync(const AsyncResource &resource, int timeout, const CompletionFn &complfn) override;


	virtual bool serve();

	virtual void releaseThreads();


	AsyncProvider();



public:
	static RefCntPtr<AsyncProvider> create(unsigned int numThreads=1, unsigned int numListeners=1);

	void setCountOfListeners(unsigned int count);

	void setCountOfThreads(unsigned int count);

	void stop();

	~AsyncProvider();


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




typedef RefCntPtr<AsyncProvider> PAsyncProvider;


}
