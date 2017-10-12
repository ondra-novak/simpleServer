/*
 * linuxWaitingSlot.cpp
 *
 *  Created on: 12. 10. 2017
 *      Author: ondra
 */

#include "linuxWaitingSlot.h"

namespace simpleServer {

LinuxWaitingSlot::LinuxWaitingSlot() {
	int fds[2];
	pipe2(pipes, O_CLOEXEC);
	intrHandle = fds[1];
	intrWaitHandle = fd[0];
	addServiceTask(fds[0]);
}

LinuxWaitingSlot::~LinuxWaitingSlot() {
	close(intrHandle);
	close(intrWaitHandle);
}

template<typename Fn>
void LinuxWaitingSlot::addTaskToQueue(int s, const Fn &fn, int timeout, int event) {
	TaskInfo nfo;
	nfo.taskFn = fn;
	nfo.timeout = timeout;
	pollfd fd;
	fd.fd = s;
	fd.events = event;
	fd.revents = 0;
	{
	std::lock_guard<std::mutex> _(queueLock);
	queueLock.push(TaskAddRequest(fd, nfo));
	}
	sendIntr(cmdQueue);

}

void LinuxWaitingSlot::read(const AsyncResource& resource,
		MutableBinaryView buffer, int timeout, Callback completion) {

	int s = resource.socket;
	auto fn = [=]{
		try {
			int r = recv(s, buffer.data, buffer.length, MSG_DONTWAIT);
			if (r > 0) {
				completion(asyncOK, BinaryView(buffer.data, r));
			} else if (r == 0) {
				completion(asyncEOF, BinaryView(0,0));
			} else {
				int e = errno;
				if (e == EWOULDBLOCK || e == EAGAIN) {
					read(AsyncResource(s), buffer,timeout,completion);
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

void LinuxWaitingSlot::write(const AsyncResource& resource, BinaryView buffer,
		int timeout, Callback completion) {

	int s = resource.socket;
	auto fn = [=]{
		try {
			int r = send(s, buffer.data, buffer.length, MSG_DONTWAIT);
			if (r > 0) {
				completion(asyncOK, buffer.substr(r));
			} else {
				int e = errno;
				if (e == EWOULDBLOCK || e == EAGAIN) {
					read(AsyncResource(s), buffer,timeout,completion);
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

LinuxWaitingSlot::Task LinuxWaitingSlot::waitForEvent() {


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
					Task t (taskMap[i].taskFn,wrTimeout);
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
									std::lock_guard<std::mutex> _(queuelock);
									if (!queue.empty()) {
										addTask(queue.front());
										queue.pop();
									}
								}
								break;
							}
						}
					} else {
						Task t (taskMap[i].taskFn,wrEvent);
						removeTask(i);
						return t;
					}
				}
			}
		}
	}
	while (true);
}

void LinuxWaitingSlot::cancelWait() {
	sendIntr(cmdExit);
}


void LinuxWaitingSlot::removeTask(int index) {
	std::size_t end = taskMap.size()-1;
	if (index < taskMap.size()-1) {
		std::swap(taskMap[index], task[end]);
		std::swap(fdmap[index],fdmap[end]);
	}
	taskMap.resize(end);
	fdmap.resize(end);
}

void LinuxWaitingSlot::addServiceTask(int fd) {
	TaskInfo nfo;
	nfo.taskFn = nullptr;
	nfo.timeout = TimePoint::max();
	pollfd xfd;
	xfd.fd = fd;
	xfd.events = POLLIN;
	xfd.revents = 0;
	taskMap.push_back(TaskInfo()
}

void LinuxWaitingSlot::sendIntr() {
}

} /* namespace simpleServer */
