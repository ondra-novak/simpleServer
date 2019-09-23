/*
 * linuxWaitingSlot.cpp
 *
 *  Created on: 12. 10. 2017
 *      Author: ondra
 */


#include <fcntl.h>
#include <sys/socket.h>

#include <unistd.h>

#include "../exceptions.h"
#include "../mt.h"
#include "netEventDispatcher.h"
#include "../defer.h"

namespace simpleServer {

using ondra_shared::defer;

static LinuxEventDispatcher::Task empty_task([](AsyncState){},asyncOK);


LinuxEventDispatcher::LinuxEventDispatcher() {
	int fds[2];
	if (pipe2(fds, O_CLOEXEC)!=0) {
		int err = errno;
		throw SystemException(err,"Failed to call pipe2 (LinuxEventDispatcher)");
	}
	intrHandle = fds[1];
	intrWaitHandle = fds[0];


	addIntrWaitHandle();

	exitFlag = false;
}

LinuxEventDispatcher::~LinuxEventDispatcher() noexcept {
	close(intrHandle);
	close(intrWaitHandle);
}

template<typename T>
void ignore_variable(T) {}

LinuxEventDispatcher::Task LinuxEventDispatcher::runQueue() {
	std::lock_guard<std::mutex> _(queueLock);
	char b;
	ignore_variable(::read(intrWaitHandle, &b, 1));



	if (!queue.empty()) {
		const RegReq &r = queue.front();
		if (r.ares.socket == 0 && r.ares.op == 0) {
			Task t(r.extra.completionFn,asyncOK);
			queue.pop();
			return t;
		} else if (r.extra.completionFn == nullptr) {
			auto q = r.ares;
			queue.pop();
			return findAndCancel(q);
		} else {
			addResource(r);
			queue.pop();
		}
	}
	return Task();
}

LinuxEventDispatcher::Task LinuxEventDispatcher::findAndCancel(const AsyncResource &res) {
	CompletionFn curFn = nullptr;
	int cnt = (int)fdmap.size();
	for (int i = 0; i < cnt; i++) {
		if (fdmap[i].fd == res.socket && fdmap[i].events == res.op) {
			if (curFn ==nullptr) curFn = fdextramap[i].completionFn;
			else {
				CompletionFn otherFn = fdextramap[i].completionFn;
				curFn = [curFn,otherFn](AsyncState st) {
					curFn(st);
					otherFn(st);
				};
			}
			deleteResource(i);
			--i;
			--cnt;
		}
	}
	return Task(curFn,asyncCancel);

}
void LinuxEventDispatcher::addIntrWaitHandle() {

	RegReq rq;
	rq.ares = AsyncResource(intrWaitHandle, POLLIN);
	rq.extra.completionFn = empty_task;
	rq.extra.timeout = TimePoint::max();
	addResource(rq);

}

void LinuxEventDispatcher::runAsync(const AsyncResource &resource, int timeout, CompletionFn &&complfn) {
	if (exitFlag || complfn == nullptr) {
		defer >> std::bind(std::move(complfn), asyncCancel);
		return;
	}

	RegReq req;
	req.ares = resource;
	req.extra.completionFn = complfn;
	if (timeout < 0) req.extra.timeout = TimePoint::max();
	else req.extra.timeout = TimePoint::clock::now() + std::chrono::milliseconds(timeout);

	std::lock_guard<std::mutex> _(queueLock);
	queue.push(req);
	sendIntr();

}

void LinuxEventDispatcher::runAsync(CustomFn &&completion)  {
	if (exitFlag || completion == nullptr) {
		defer >> completion;
		return;
	}


	RegReq req;
	req.extra.completionFn = [fn=std::move(completion)](AsyncState){fn();};

	std::lock_guard<std::mutex> _(queueLock);
	queue.push(req);
	sendIntr();
}

void LinuxEventDispatcher::addResource(const RegReq &req) {

	pollfd pfd;
	pfd.events = req.ares.op;
	pfd.fd = req.ares.socket;
	pfd.revents = 0;
	fdmap.push_back(pfd);
	fdextramap.push_back(req.extra);
	if (req.extra.timeout < nextTimeout) nextTimeout = req.extra.timeout;
}
void LinuxEventDispatcher::deleteResource(int index) {
	int last = fdmap.size()-1;
	if (index < last) {
		std::swap(fdmap[index],fdmap[last]);
		std::swap(fdextramap[index],fdextramap[last]);
	}
	fdmap.resize(last);
	fdextramap.resize(last);
}


LinuxEventDispatcher::Task LinuxEventDispatcher::checkEvents(const TimePoint &now, bool finish) {
	int sz = static_cast<int>(fdmap.size());
	if (last_checked >= sz) {
		if (finish) return Task();
		last_checked = 0;
		nextTimeout = TimePoint::max();
	}

	while (last_checked < sz) {
		int idx = last_checked++;
		if (fdmap[idx].revents) {
			if (fdmap[idx].fd == intrWaitHandle) {
				Task t = runQueue();
				fdmap[idx].revents = 0;
				if (t != nullptr) return t;
			} else {
				Task t(fdextramap[idx].completionFn, asyncOK);
				deleteResource(idx);
				--last_checked;
				return t;
			}
		} else if (fdextramap[idx].timeout<=now) {
			Task t(fdextramap[idx].completionFn, asyncTimeout);
			deleteResource(idx);
			--last_checked;
			return t;
		} else if (fdextramap[idx].timeout<nextTimeout) {
			nextTimeout = fdextramap[idx].timeout;
		}
	}
	return Task();
}

///returns true, if the listener doesn't contain any asynchronous task
bool LinuxEventDispatcher::empty() const {
	return fdmap.empty();
}

void LinuxEventDispatcher::stop() {
	exitFlag = true;
	sendIntr();
}

unsigned int LinuxEventDispatcher::getPendingCount() const {
	return fdmap.size();
}

void LinuxEventDispatcher::cancel(const AsyncResource& resource) {
	RegReq req;
	req.ares = resource;
	req.extra.completionFn = nullptr;

	std::lock_guard<std::mutex> _(queueLock);
	queue.push(req);
	sendIntr();

}

LinuxEventDispatcher::Task LinuxEventDispatcher::cleanup() {
	if (!fdmap.empty()) {
		int idx = fdmap.size()-1;
		Task t(fdextramap[idx].completionFn, asyncCancel);
		deleteResource(idx);
		return t;
	} else {
		return Task();
	}

}

LinuxEventDispatcher::Task LinuxEventDispatcher::wait() {


	if (exitFlag) {
		return cleanup();
	}

	TimePoint now = std::chrono::steady_clock::now();
	Task x = checkEvents(now,true);
	if (x != nullptr) return x;

	auto int_ms = std::chrono::duration_cast<std::chrono::milliseconds>(nextTimeout - now);
	int r = poll(fdmap.data(),fdmap.size(),int_ms.count());


	if (r < 0) {
		int e = errno;
		if (e != EINTR && e != EAGAIN)
			throw SystemException(e, "Failed to call poll()");
	} else {
		now = std::chrono::steady_clock::now();
		x = checkEvents(now,false);
		if (x != nullptr) return x;
	}
	return empty_task;
}


void LinuxEventDispatcher::sendIntr() {
	unsigned char b = 1;
	int r = ::write(intrHandle, &b, 1);
	if (r < 0) {
		throw SystemException(errno);
	}
}

PStreamEventDispatcher AbstractStreamEventDispatcher::create() {
	return new LinuxEventDispatcher;
}


} /* namespace simpleServer */

