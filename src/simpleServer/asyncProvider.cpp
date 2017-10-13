#include "asyncProvider.h"

#include <thread>

namespace simpleServer {


AsyncProvider::~AsyncProvider() {
	stop();
}

bool AsyncProvider::serve() {
	PEventListener lst = tQueue.pop();
	if (lst == nullptr) {
		tQueue.push(nullptr);
		return;
	}
	auto t = lst->waitForEvent();
	tQueue.push(lst);
	if (t != nullptr) {
		t();
		return true;
	} else {
		return false;
	}

}

void AsyncProvider::releaseThreads() {
	tQueue.push(nullptr);
	Sync _(lock);
	while (!cQueue.empty()) {
		auto l = cQueue.front();
		cQueue.pop();
		l->releaseThreads();
	}
}

PEventListener AsyncProvider::getListener() {
	Sync _(lock);
	PEventListener lst;
	while (cQueue.size() < reqListenerCount) {
		lst = AbstractStreamEventDispatcher::create();
		cQueue.push(lst);
		tQueue.push(lst);
	}
	while (threadCount.getCounter() < reqThreadCount) {
		PAsyncProvider me (this);
		std::thread thr([me]{me->worker();});
		thr.detach();
		threadCount.inc();
	}
	lst = cQueue.front();
	cQueue.pop();
	cQueue.push(lst);
	return lst;
}



void AsyncProvider::receive(const AsyncResource& resource,
		MutableBinaryView buffer, int timeout, Callback completion) {

	auto lst = getListener();
	lst->receive(resource,buffer,timeout,completion);
}

void AsyncProvider::send(const AsyncResource& resource,
		BinaryView buffer, int timeout, Callback completion) {

	auto lst = getListener();
	lst->send(resource,buffer,timeout,completion);
}

RefCntPtr<AsyncProvider> AsyncProvider::create(unsigned int numThreads, unsigned int numListeners) {
	RefCntPtr<AsyncProvider> provider = new AsyncProvider;
	provider->setCountOfListeners(numListeners);
	provider->setCountOfThreads(numThreads);
}

void AsyncProvider::setCountOfListeners(unsigned int count) {
	Sync _(lock);
	reqListenerCount = count;
}

void AsyncProvider::setCountOfThreads(unsigned int count) {
	Sync _(lock);
	reqThreadCount = count;
}

void AsyncProvider::stop() {
	releaseThreads();
	threadCount.zeroWait();

}

AsyncProvider::AsyncProvider() {
}

void AsyncProvider::worker() {
	for(;;) {
		PEventListener lst = tQueue.pop();
		if (lst == nullptr) {
			tQueue.push(nullptr);
			return;
		}


		auto t = lst->waitForEvent();
		tQueue.push(lst);
		if (t == nullptr) {
			threadCount.dec();
			return;
		}
		t();

		if (threadCount.getCounter() > reqThreadCount) {
			Sync _(lock);
			if (threadCount.getCounter() > reqThreadCount) {
				threadCount.dec();
				return;
			}
		}
	}
}


}
