#include "threadPoolAsync.h"
#include "shared/defer.tcc"

#include <thread>

#include "../simpleServer/mt.h"

#include "exceptions.h"

namespace simpleServer {


ThreadPoolAsyncImpl::~ThreadPoolAsyncImpl() {
	stop();
}

void ThreadPoolAsyncImpl::checkThreadCount() {
	while (static_cast<unsigned int>(threadCount.getCounter()) < reqThreadCount) {
		RefCntPtr<ThreadPoolAsyncImpl> me(this);
		runThread([me] {
			me->worker();
		});
		threadCount.inc();
	}
}

class ThreadPoolAsyncImpl::InvokeNetworkDispatcher {
public:
	InvokeNetworkDispatcher(ThreadPoolAsyncImpl &owner, const PStreamEventDispatcher &sed)
		:owner(owner)
		,sed(sed) {}
	void operator()() const {
/*		auto t = sed->waitForEvent();
		if (t == nullptr) {
			disp.quit();
		} else {
			disp.dispatch(*this);
		}
		t();*/
		owner.waitForTask(sed);
	}
protected:
	ThreadPoolAsyncImpl &owner;
	PStreamEventDispatcher sed;
};


void ThreadPoolAsyncImpl::waitForTask(const PStreamEventDispatcher &sed) noexcept {
			auto t = sed->waitForEvent();
			if (t == nullptr) {
				dQueue.quit();
				return;
			} else if (!sed->empty() || sed->isShared()) {
				dQueue.dispatch(InvokeNetworkDispatcher(*this, sed));
			}
			t();
}

PStreamEventDispatcher ThreadPoolAsyncImpl::getListener() {
	Sync _(lock);
	PStreamEventDispatcher lst,lst2;
	while (cQueue.size() < reqDispatcherCount) {
		lst = AbstractStreamEventDispatcher::create();
		cQueue.push(lst);
		dQueue.dispatch(InvokeNetworkDispatcher(*this, lst));
	}
	while (cQueue.size() > reqDispatcherCount) {
		//pop extra dispatcher
		//they are still in queue, but eventually disappear because they no longer receive a work
		cQueue.pop();
	}
	checkThreadCount();
	lst = cQueue.front();
	cQueue.pop();
	cQueue.push(lst);
	return lst;
}


void ThreadPoolAsyncImpl::runAsync(const AsyncResource& resource, int timeout, const CompletionFn &fn) {
	unsigned int tries = 0;
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

	dQueue.quit();
	{
		Sync _(lock);
		while (!cQueue.empty()) {
			auto l = cQueue.front();
			cQueue.pop();
			l->stop();
		}
	}
	threadCount.wait();

}

void ThreadPoolAsyncImpl::setTasksPerDispLimit(unsigned int count) {
	Sync _(lock);
	taskLimit = count;

}

void ThreadPoolAsyncImpl::worker() noexcept {


	using namespace ondra_shared;

	DeferContext defer(ondra_shared::defer_root);

	for(;;) {


		if (!dQueue.pump()) {
			dQueue.quit();
			threadCount.dec();
			return;
		}

		defer_yield();
/*		PStreamEventDispatcher lst = tQueue.pop();
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
*/
		if (static_cast<unsigned int>(threadCount.getCounter()) > reqThreadCount) {
			Sync _(lock);
			if (static_cast<unsigned int>(threadCount.getCounter()) > reqThreadCount) {
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

void ThreadPoolAsyncImpl::runAsync(const CustomFn& completion) {
	if (reqThreadCount > reqDispatcherCount) {
		checkThreadCount();
		dQueue.dispatch(completion);
	} else {
		auto lst = getListener();
		lst->runAsync(completion);
	}
}

void ThreadPoolAsync::setTasksPerDispLimit(unsigned int count) {
	ptr->setTasksPerDispLimit(count);
}

ThreadPoolAsync::operator AsyncProvider() const {
	return AsyncProvider(ptr);
}

}

