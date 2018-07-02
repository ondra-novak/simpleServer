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
#include "netEventDispatcher.h"

namespace simpleServer {

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

void LinuxEventDispatcher::addIntrWaitHandle() {

	RefCntPtr<LinuxEventDispatcher> me(this);
	auto completionFn = [me](AsyncState) {
		char b;
		::read(me->intrWaitHandle, &b, 1);

		std::lock_guard<std::mutex> _(me->queueLock);
		if (!me->queue.empty()) {
			const RegReq &r = me->queue.front();
			if (r.ares.socket == 0 && r.ares.socket == 0) {
				r.extra.completionFn(asyncOK);
			} else {
				me->addResource(r);
				me->queue.pop();
			}
		}
		me->addIntrWaitHandle();
	};

	RegReq rq;
	rq.ares = AsyncResource(intrWaitHandle, POLLIN);
	rq.extra.completionFn = completionFn;
	rq.extra.timeout = TimePoint::clock::now();

}

void LinuxEventDispatcher::runAsync(const AsyncResource &resource, int timeout, const CompletionFn &complfn) {
	RegReq req;
	req.ares = resource;
	req.extra.completionFn = complfn;
	if (timeout < 0) req.extra.timeout = TimePoint::max();
	else req.extra.timeout = TimePoint::clock::now() + std::chrono::milliseconds(timeout);

	std::lock_guard<std::mutex> _(queueLock);
	queue.push(req);
	sendIntr();

}

void LinuxEventDispatcher::runAsync(const CustomFn &completion)  {
	RegReq req;
	req.extra.completionFn = [fn=CustomFn(completion)](AsyncState){fn();};

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

static LinuxEventDispatcher::Task empty_task([](AsyncState){},asyncOK);

LinuxEventDispatcher::Task LinuxEventDispatcher::checkEvents(const TimePoint &now) {
	if (last_checked >= static_cast<int>(fdmap.size())) {
		last_checked = 0;
		nextTimeout = TimePoint::max();
	}

	while (last_checked < static_cast<int>(fdmap.size())) {
		int idx = last_checked++;
		if (fdmap[idx].revents) {
			Task t(fdextramap[idx].completionFn, asyncOK);
			deleteResource(idx);
			--last_checked;
			return t;
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


LinuxEventDispatcher::Task LinuxEventDispatcher::cleanup() {
	if (!fdmap.empty()) {
		int idx = fdmap.size()-1;
		Task t(fdextramap[idx].completionFn, asyncError);
		deleteResource(idx);
		return t;
	} else {
		return Task();
	}

}

LinuxEventDispatcher::Task LinuxEventDispatcher::waitForEvent() {


	if (exitFlag) {
		return cleanup();
	}

	TimePoint now = std::chrono::steady_clock::now();
	if (last_checked >= 0 || now > nextTimeout) {
		Task x = checkEvents(now);
		if (x != nullptr) return x;
	}

	auto int_ms = std::chrono::duration_cast<std::chrono::milliseconds>(nextTimeout - now);
	int r = poll(fdmap.data(),fdmap.size(),int_ms.count());


	if (r < 0) {
		int e = errno;
		if (e != EINTR && e != EAGAIN)
			throw SystemException(e, "Failed to call poll()");
	} else {
		now = std::chrono::steady_clock::now();
		Task x = checkEvents(now);
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

