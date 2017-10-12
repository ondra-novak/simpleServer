
#pragma once

#include "abstractAsyncProvider.h"

namespace simpleServer {


class AsyncProvider: public IAsyncProvider {
public:


	AsyncProvider(int evListenerCount);

	virtual void receive(const AsyncResource &resource,
			MutableBinaryView buffer,
			int timeout,
			Callback completion);

	virtual void send(const AsyncResource &resource,
			BinaryView buffer,
			int timeout,
			Callback completion);

	virtual bool serve();

	virtual void releaseThreads();








protected:
	MsgQueue<PEventListener> tQueue, cQueue;



};


}
