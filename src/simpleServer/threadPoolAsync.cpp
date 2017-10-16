#include "threadPoolAsync.h"

#include <thread>

namespace simpleServer {


ThreadPoolAsyncImpl::~ThreadPoolAsyncImpl() {
	stop();
}

PEventListener ThreadPoolAsyncImpl::getListener() {
	Sync _(lock);
	PEventListener lst;
	while (cQueue.size() < reqListenerCount) {
		lst = AbstractStreamEventDispatcher::create();
		cQueue.push(lst);
		tQueue.push(lst);
	}
	while (threadCount.getCounter() < reqThreadCount) {
		RefCntPtr<ThreadPoolAsyncImpl> me (this);
		std::thread thr([me]{me->worker();});
		thr.detach();
		threadCount.inc();
	}
	lst = cQueue.front();
	cQueue.pop();
	cQueue.push(lst);
	return lst;
}


void ThreadPoolAsyncImpl::runAsync(const AsyncResource& resource, int timeout, const CompletionFn &fn) {
	auto lst = getListener();
	lst->runAsync(resource,timeout, fn);

}


void ThreadPoolAsyncImpl::setCountOfListeners(unsigned int count) {
	Sync _(lock);
	reqListenerCount = count;
}

void ThreadPoolAsyncImpl::setCountOfThreads(unsigned int count) {
	Sync _(lock);
	reqThreadCount = count;
}

void ThreadPoolAsyncImpl::stop() {

	tQueue.push(nullptr);
	{
		Sync _(lock);
		while (!cQueue.empty()) {
			auto l = cQueue.front();
			cQueue.pop();
			l->stop();
		}
	}
	threadCount.zeroWait();

}

ThreadPoolAsyncImpl::ThreadPoolAsyncImpl() {
}

void ThreadPoolAsyncImpl::worker() {
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

AsyncProvider ThreadPoolAsync::create(unsigned int numThreads, unsigned int numListeners) {
	ThreadPoolAsync provider (new ThreadPoolAsyncImpl);
	provider.setCountOfListeners(numListeners);
	provider.setCountOfThreads(numThreads);
	return provider;
}

void ThreadPoolAsync::setCountOfListeners(unsigned int count) {
	ptr->setCountOfListeners(count);
}

void ThreadPoolAsync::setCountOfThreads(unsigned int count) {
	ptr->setCountOfThreads(count);
}

void ThreadPoolAsync::stop() {
	ptr->stop();
}

ThreadPoolAsync::~ThreadPoolAsync() {
	if (!ptr->isShared()) {
		ptr->stop();
	}
}

void ThreadPoolAsyncImpl::runAsync(const CompletionFn& completion) {
}

ThreadPoolAsync::operator AsyncProvider() const {
	return AsyncProvider(ptr);
}

}

