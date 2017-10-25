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
	pipe2(fds, O_CLOEXEC);
	intrHandle = fds[1];
	intrWaitHandle = fds[0];

	pollfd xfd;
	xfd.fd = intrWaitHandle;
	xfd.events = POLLIN;
	xfd.revents = 0;
	fdmap.push_back(xfd);

	exitFlag = false;
}

LinuxEventDispatcher::~LinuxEventDispatcher() {
	close(intrHandle);
	close(intrWaitHandle);
}

void LinuxEventDispatcher::addTaskToQueue(int s, const CompletionFn &fn, int timeout, int event) {
	TaskInfo nfo;
	nfo.taskFn = fn;
	nfo.timeout = timeout == -1?TimePoint::max():std::chrono::steady_clock::now()+std::chrono::milliseconds(timeout);
	nfo.org_timeout = timeout;
	pollfd fd;
	fd.fd = s;
	fd.events = event;
	fd.revents = 0;

	bool sintr;

	{
		std::lock_guard<std::mutex> _(queueLock);
		sintr = queue.empty();
		queue.push(TaskAddRequest(fd, nfo));
	}
	if (sintr)
		sendIntr();

}


LinuxEventDispatcher::Task LinuxEventDispatcher::waitForEvent() {


	if (exitFlag) {
		epilog();
		return nullptr;
	}
	Task ret = runQueue();
	if (ret != nullptr) return ret;

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
						if (exitFlag) {
							epilog();
							return nullptr;
						}
						ret = runQueue();
						if (ret == nullptr) return []{};
						return ret;
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


void LinuxEventDispatcher::stop() {
	exitFlag = true;
	sendIntr();
}


void LinuxEventDispatcher::removeTask(int index, TaskMap::iterator &iter) {
	std::size_t end = fdmap.size()-1;
	if (index < fdmap.size()-1) {
		std::swap(fdmap[index],fdmap[end]);
	}
	fdmap.resize(end);
	taskMap.erase(iter);
}

void LinuxEventDispatcher::runAsync(const AsyncResource& resource, int timeout,const CompletionFn &complfn) {

	int s = resource.socket;
	int op = resource.op;
	addTaskToQueue(s,complfn,timeout,op);

}

bool LinuxEventDispatcher::empty() const {
	return taskMap.empty();
}

unsigned int LinuxEventDispatcher::getPendingCount() const {
	std::lock_guard<std::mutex> _(queueLock);
	return fdmap.size()-1;

}

void LinuxEventDispatcher::epilog() {
	std::lock_guard<std::mutex> _(queueLock);
	if (moveToProvider) {

		auto e = taskMap.end();
		for (auto &&f: fdmap) {
			auto itr = taskMap.find(RKey(f.fd,f.events));
			if (itr != e) {
				moveToProvider.runAsync(AsyncResource(f.fd, f.events), itr->second.org_timeout, itr->second.taskFn);
			}
		}
		moveToProvider = nullptr;
	}

	taskMap.clear();
	fdmap.clear();
}

void LinuxEventDispatcher::moveTo(AsyncProvider target) {
	std::lock_guard<std::mutex> _(queueLock);
	moveToProvider = target;
	stop();
}

LinuxEventDispatcher::Task LinuxEventDispatcher::addTask(const TaskAddRequest& req) {
	if (req.first.fd == -1) {
		CompletionFn fn(req.second.taskFn);
		return [fn] {fn(asyncOK);};
	} else {
		RKey kk(req.first.fd, req.first.events);
		auto itr = taskMap.find(kk);
		if (itr != taskMap.end()) {
			itr->second = req.second;
		} else {
			fdmap.push_back(req.first);
			taskMap.insert(std::make_pair(kk, req.second));
		}
		return nullptr;
	}
}

void LinuxEventDispatcher::runAsync(const CompletionFn& completion) {
	addTaskToQueue(-1, completion,0,0);
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

std::size_t LinuxEventDispatcher::HashRKey::operator ()(const RKey& key) const {
	return key.first*16+key.second;
}

LinuxEventDispatcher::Task LinuxEventDispatcher::runQueue() {

	Task s;
	std::lock_guard<std::mutex> _(queueLock);
	while (!queue.empty() && s==nullptr) {
		auto &&x = queue.front();
		 s = addTask(x);
		if (x.second.timeout < nextTimeout) {
			nextTimeout = x.second.timeout;
		}
		queue.pop();
	}
	return s;
}


} /* namespace simpleServer */

