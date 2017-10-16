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
	clear();
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
	nfo.org_timeout = timeout;
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



	Task ret = runQueue();
	if (ret != nullptr) return ret;

	do {
		TimePoint n = std::chrono::steady_clock::now();
		auto tme = taskMap.end();

		if (n > nextTimeout) {
			nextTimeout = TimePoint::max();
			for (auto &&x : taskMap)
				if (x.second.timeout < nextTimeout) nextTimeout = x.second.timeout;
		}

		auto int_ms = std::chrono::duration_cast<std::chrono::milliseconds>(nextTimeout - n);

		int r = poll(fdmap.data(),fdmap.size(),int_ms.count());
		if (r < 0) {
			int e = errno;
			if (e != EINTR && e != EAGAIN)
				throw SystemException(e, "Failed to call poll()");
		} else if (r == 0) {
			n = std::chrono::steady_clock::now();
			for (std::size_t i = 0, cnt = fdmap.size(); i < cnt; i++) {
				auto tmi = taskMap.find(RKey(fdmap[i].fd,fdmap[i].events));
				if (tmi != tme) {
					Task t = std::bind(tme->second.taskFn,asyncTimeout);
					removeTask(i,tmi);
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
								case cmdQueue:
									ret = runQueue();
									if (ret != nullptr) return ret;
									break;
							}
						}
					} else {
						auto tmi = taskMap.find(RKey(fdmap[i].fd,fdmap[i].events));
						if (tmi != tme) {
							Task t (std::bind(tmi->second.taskFn,asyncOK));
							removeTask(i,tmi);
							return t;
						}
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


void LinuxEventListener::removeTask(int index, TaskMap::iterator &iter) {
	std::size_t end = taskMap.size()-1;
	if (index < taskMap.size()-1) {
		std::swap(fdmap[index],fdmap[end]);
	}
	fdmap.resize(end);
	taskMap.erase(iter);
}

void LinuxEventListener::runAsync(const AsyncResource& resource, int timeout,const CompletionFn &complfn) {

	int s = resource.socket;
	int op = resource.op;
	addTaskToQueue(s,complfn,timeout,op);

}

bool LinuxEventListener::empty() const {
	return taskMap.empty();
}

void LinuxEventListener::clear() {
	taskMap.clear();
	fdmap.clear();

	pollfd xfd;
	xfd.fd = intrWaitHandle;
	xfd.events = POLLIN;
	xfd.revents = 0;
	fdmap.push_back(xfd);

}

void LinuxEventListener::moveTo(AbstractStreamEventDispatcher& target) {
	auto e = taskMap.end();
	for (auto &&f: fdmap) {
		auto itr = taskMap.find(RKey(f.fd,f.events));
		if (itr != e) {
			target.runAsync(AsyncResource(f.fd, f.events), itr->second.org_timeout, itr->second.taskFn);
		}
	}
	clear();
}

LinuxEventListener::Task LinuxEventListener::addTask(const TaskAddRequest& req) {
	if (req.first.fd == -1) {
		CompletionFn fn(req.second.taskFn);
		return [fn] {fn(asyncOK);};
	} else {
		RKey kk(req.first.fd, req.first.events);
		auto itr = taskMap.find(kk);
		if (itr == taskMap.end()) {
			itr->second = req.second;
		} else {
			fdmap.push_back(req.first);
			taskMap.insert(std::make_pair(kk, req.second));
		}
		return nullptr;
	}
}

void LinuxEventListener::runAsync(const CompletionFn& completion) {
	addTaskToQueue(-1, completion,0,0);
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

std::size_t LinuxEventListener::HashRKey::operator ()(const RKey& key) const {
	return key.first*16+key.second;
}

LinuxEventListener::Task LinuxEventListener::runQueue() {
	std::lock_guard<std::mutex> _(queueLock);
	while (!queue.empty()) {
		auto &&x = queue.front();
		Task s = addTask(x);
		if (x.second.timeout < nextTimeout) {
			nextTimeout = x.second.timeout;
		}
		queue.pop();
		if (s != nullptr) return s;
	}
	return nullptr;
}

} /* namespace simpleServer */

