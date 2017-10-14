/*
 * linuxWaitingSlot.cpp
 *
 *  Created on: 12. 10. 2017
 *      Author: ondra
 */

#include "linuxWaitingSlot.h"

#include <fcntl.h>
#include <sys/socket.h>

#include <unistd.h>

#include "../exceptions.h"

namespace simpleServer {

LinuxEventListener::LinuxEventListener() {
	int fds[2];
	pipe2(fds, O_CLOEXEC);
	intrHandle = fds[1];
	intrWaitHandle = fds[0];
	TaskInfo nfo;
	nfo.taskFn = nullptr;
	nfo.timeout = TimePoint::max();
	pollfd xfd;
	xfd.fd = intrWaitHandle;
	xfd.events = POLLIN;
	xfd.revents = 0;
	taskMap.push_back(nfo);
	fdmap.push_back(xfd);
}

LinuxEventListener::~LinuxEventListener() {
	close(intrHandle);
	close(intrWaitHandle);
}

template<typename Fn>
void LinuxEventListener::addTaskToQueue(int s, const Fn &fn, int timeout, int event) {
	TaskInfo nfo;
	nfo.taskFn = fn;
	nfo.timeout = timeout == -1?TimePoint::max():std::chrono::steady_clock::now()+std::chrono::milliseconds(timeout);
	pollfd fd;
	fd.fd = s;
	fd.events = event;
	fd.revents = 0;
	{
	std::lock_guard<std::mutex> _(queueLock);
	queue.push(TaskAddRequest(fd, nfo));
	}
	sendIntr(cmdQueue);

}

/*
void LinuxEventListener::receive(const AsyncResource& resource,
		MutableBinaryView buffer, int timeout, Callback completion) {

	int s = resource.socket;
	auto fn = [=](WaitResult res){
		try {
			int r = ::recv(s, buffer.data, buffer.length, MSG_DONTWAIT);
			if (r > 0) {
				completion(asyncOK, BinaryView(buffer.data, r));
			} else if (r == 0) {
				completion(asyncEOF, BinaryView(0,0));
			} else {
				int e = errno;
				if (e == EWOULDBLOCK || e == EAGAIN) {
					receive(AsyncResource(s), buffer,timeout,completion);
				} else {
						throw SystemException(e, "Async recv error");
				}
			}
		} catch (...) {
			completion(asyncError, BinaryView(0,0));
		}
	};
	addTaskToQueue(s, fn, timeout, POLLIN);

}

void LinuxEventListener::send(const AsyncResource& resource, BinaryView buffer,
		int timeout, Callback completion) {

	int s = resource.socket;
	auto fn = [=](WaitResult res){
		try {
			int r = ::send(s, buffer.data, buffer.length, MSG_DONTWAIT);
			if (r > 0) {
				completion(asyncOK, buffer.substr(r));
			} else {
				int e = errno;
				if (e == EWOULDBLOCK || e == EAGAIN) {
					send(AsyncResource(s), buffer,timeout,completion);
				} else {
						throw SystemException(e, "Async send error");
				}
			}
		} catch (...) {
			completion(asyncError, BinaryView(0,0));
		}
	};
	addTaskToQueue(s, fn, timeout, POLLOUT);
}

*/
LinuxEventListener::Task LinuxEventListener::waitForEvent() {


	do {
		TimePoint n = std::chrono::steady_clock::now();

		if (n > nextTimeout) {
			nextTimeout = TimePoint::max();
			for (auto &&x : taskMap)
				if (x.timeout < nextTimeout) nextTimeout = x.timeout;
		}

		auto int_ms = std::chrono::duration_cast<std::chrono::milliseconds>(nextTimeout - n);

		int r = poll(fdmap.data(),fdmap.size(),int_ms.count());
		if (r < 0) {
			int e = errno;
			if (e != EINTR && e != EAGAIN)
				throw SystemException(e, "Failed to call poll()");
		} else if (r == 0) {
			n = std::chrono::steady_clock::now();
			for (std::size_t i = 0, cnt = taskMap.size(); i < cnt; i++) {
				if (n > taskMap[i].timeout) {
					Task t = std::bind(taskMap[i].taskFn,asyncTimeout);
					removeTask(i);
					return t;
				}
			}
		} else {
			for (std::size_t i = 0, cnt = fdmap.size(); i < cnt; ++i) {
				pollfd &fd = fdmap[i];
				if (fd.revents) {
					if (fd.fd == intrWaitHandle) {
						fd.revents = 0;
						unsigned char b;
						int r = ::read(fd.fd,&b,1);
						if (r==1) {
							Command cmd = (Command)b;
							switch (b) {
								case cmdExit: return nullptr;
								case cmdQueue:{
									std::lock_guard<std::mutex> _(queueLock);
									if (!queue.empty()) {
										auto &&x = queue.front();
										addTask(x);
										queue.pop();
										if (x.second.timeout < nextTimeout) {
											nextTimeout = x.second.timeout;
										}
									}
								}
								break;
							}
						}
					} else {
						Task t (std::bind(taskMap[i].taskFn,asyncOK));
						removeTask(i);
						return t;
					}
				}
			}
		}
	}
	while (true);
}

void LinuxEventListener::cancelWait() {
	sendIntr(cmdExit);
}


void LinuxEventListener::removeTask(int index) {
	std::size_t end = taskMap.size()-1;
	if (index < taskMap.size()-1) {
		taskMap[index].swap(taskMap[end]);
		std::swap(fdmap[index],fdmap[end]);
	}
	taskMap.resize(end);
	fdmap.resize(end);
}

void LinuxEventListener::runAsync(const AsyncResource& resource, int timeout,const CompletionFn &complfn) {

	int s = resource.socket;
	int op = resource.op;
	addTaskToQueue(s,complfn,timeout,op);

}

void LinuxEventListener::sendIntr(Command cmd) {
	int r = ::write(intrHandle, &cmd, 1);
	if (r < 0) {
		throw SystemException(errno);
	}
}

PEventListener AbstractStreamEventDispatcher::create() {
	return new LinuxEventListener;
}


} /* namespace simpleServer */
