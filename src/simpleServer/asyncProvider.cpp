#include "asyncProvider.h"

#include <thread>

namespace simpleServer {


AsyncProviderImpl::~AsyncProviderImpl() {
	stop();
}

bool AsyncProviderImpl::serve() {
	PEventListener lst = tQueue.pop();
	if (lst == nullptr) {
		tQueue.push(nullptr);
		return false;
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

void AsyncProviderImpl::releaseThreads() {
	tQueue.push(nullptr);
	Sync _(lock);
	while (!cQueue.empty()) {
		auto l = cQueue.front();
		cQueue.pop();
		l->releaseThreads();
	}
}

PEventListener AsyncProviderImpl::getListener() {
	Sync _(lock);
	PEventListener lst;
	while (cQueue.size() < reqListenerCount) {
		lst = AbstractStreamEventDispatcher::create();
		cQueue.push(lst);
		tQueue.push(lst);
	}
	while (threadCount.getCounter() < reqThreadCount) {
		RefCntPtr<AsyncProviderImpl> me (this);
		std::thread thr([me]{me->worker();});
		thr.detach();
		threadCount.inc();
	}
	lst = cQueue.front();
	cQueue.pop();
	cQueue.push(lst);
	return lst;
}


void AsyncProviderImpl::runAsync(const AsyncResource& resource, int timeout, const CompletionFn &fn) {
	auto lst = getListener();
	lst->runAsync(resource,timeout, fn);

}


void AsyncProviderImpl::setCountOfListeners(unsigned int count) {
	Sync _(lock);
	reqListenerCount = count;
}

void AsyncProviderImpl::setCountOfThreads(unsigned int count) {
	Sync _(lock);
	reqThreadCount = count;
}

void AsyncProviderImpl::stop() {
	releaseThreads();
	threadCount.zeroWait();

}

AsyncProviderImpl::AsyncProviderImpl() {
}

void AsyncProviderImpl::worker() {
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

AsyncProvider AsyncProvider::create(unsigned int numThreads, unsigned int numListeners) {
	AsyncProvider provider (new AsyncProviderImpl);
	provider.setCountOfListeners(numListeners);
	provider.setCountOfThreads(numThreads);
	return provider;
}

void AsyncProvider::setCountOfListeners(unsigned int count) {
	ptr->setCountOfListeners(count);
}

void AsyncProvider::setCountOfThreads(unsigned int count) {
	ptr->setCountOfThreads(count);
}

void AsyncProvider::stop() {
	ptr->stop();
}

AsyncProvider::~AsyncProvider() {
	if (!ptr->isShared()) {
		ptr->stop();
	}
}

void AsyncProviderImpl::runAsync(const CompletionFn& completion) {
}


}

