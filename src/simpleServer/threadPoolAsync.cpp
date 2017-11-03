#include "threadPoolAsync.h"

#include <thread>

#include "../simpleServer/mt.h"

#include "exceptions.h"

namespace simpleServer {


ThreadPoolAsyncImpl::~ThreadPoolAsyncImpl() {
	stop();
}

PStreamEventDispatcher ThreadPoolAsyncImpl::getListener() {
	Sync _(lock);
	PStreamEventDispatcher lst;
	while (cQueue.size() < reqDispatcherCount) {
		lst = AbstractStreamEventDispatcher::create();
		cQueue.push(lst);
		tQueue.push(lst);
	}
	while (threadCount.getCounter() < reqThreadCount) {
		RefCntPtr<ThreadPoolAsyncImpl> me (this);
		runThread([me]{me->worker();});
		threadCount.inc();
	}
	lst = cQueue.front();
	cQueue.pop();
	cQueue.push(lst);
	return lst;
}


void ThreadPoolAsyncImpl::runAsync(const AsyncResource& resource, int timeout, const CompletionFn &fn) {
	int tries = 0;
	auto retry = [&] {
		Sync _(lock);
		++tries;
		if (tries >= cQueue.size())  {
			reqDispatcherCount++;
			if (reqDispatcherCount>reqThreadCount) reqThreadCount++;
		}
	};
	do {
		try {
			auto lst = getListener();
			if (lst->getPendingCount()>=taskLimit) {
				retry();
			} else {
				lst->runAsync(resource,timeout, fn);
				return;
			}
		} catch (OutOfSpaceException) {
			retry();
		}
	}while(true);
}


void ThreadPoolAsyncImpl::setCountOfDispatchers(unsigned int count) {
	Sync _(lock);
	reqDispatcherCount = count;
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

void ThreadPoolAsyncImpl::setTasksPerDispLimit(unsigned int count) {
	Sync _(lock);
	taskLimit = count;

}

void ThreadPoolAsyncImpl::worker() noexcept {
	for(;;) {

		if (cQueue.size() > reqDispatcherCount) {
			Sync _(lock);
			if (cQueue.size() > reqDispatcherCount) {
				PStreamEventDispatcher lst = cQueue.front();
				cQueue.pop();
				lst->moveTo(this);
			}
		}


		PStreamEventDispatcher lst = tQueue.pop();
		if (lst == nullptr) {
			tQueue.push(nullptr);
			threadCount.dec();
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

AsyncProvider ThreadPoolAsync::create(unsigned int numThreads, unsigned int numListeners, unsigned int tasksPerDispLimit) {
	ThreadPoolAsync provider (new ThreadPoolAsyncImpl);
	provider.setCountOfDispatchers(numListeners);
	provider.setCountOfThreads(numThreads);
	provider.setTasksPerDispLimit(tasksPerDispLimit);
	return provider;
}

void ThreadPoolAsync::setCountOfDispatchers(unsigned int count) {
	ptr->setCountOfDispatchers(count);
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
	auto lst = getListener();
	lst->runAsync(completion);
}

void ThreadPoolAsync::setTasksPerDispLimit(unsigned int count) {
	ptr->setTasksPerDispLimit(count);
}

ThreadPoolAsync::operator AsyncProvider() const {
	return AsyncProvider(ptr);
}

}

