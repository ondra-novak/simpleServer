
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include "epollAsync.h"

#include "../simpleServer/mt.h"
#include "connection.h"

#include "exceptions.h"


namespace simpleServer {


EPollAsync::EPollAsync() {
	epollfd = epoll_create1(EPOLL_CLOEXEC);

	stopped = false;

}

void EPollAsync::updateTimeout(FdReg* rg, unsigned int timeout, WaitFor wf) {
	if (rg->tmPos[wf])
		removeTimeout(rg->tmPos[wf]);
	if (timeout != infinity) {
		addTimeout(TmReg(Clock::now() + Millis(timeout), &(rg->tmPos[wf])));
	}
}

int EPollAsync::epollUpdateFd(unsigned int fd, FdReg* rg) {
	int r;
	struct epoll_event ev;
	ev.events = EPOLLONESHOT;
	if (rg->cb[wfRead] != nullptr)
		ev.events |= EPOLLIN;

	if (rg->cb[wfRead] != nullptr)
		ev.events |= EPOLLOUT;

	if (ev.events == EPOLLONESHOT) {
		epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &ev);
		rg->fd = 0;
		return 1;
	} else {
		r = epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &ev);
		return r;
	}
}

void EPollAsync::asyncWait(WaitFor wf, unsigned int fd, unsigned int timeout, CallbackFn fn) {
	Sync _(regLock);

	struct epoll_event ev;
	ev.data.fd = fd;
	ev.events = (wf==wfRead?EPOLLIN:EPOLLOUT)|EPOLLONESHOT;
	int r = epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&ev);
	if (r == -1) {
		int e = errno;
		if (e == EEXIST) {

			FdReg *rg = findReg(fd);
			rg->cb[wf] = fn;
			r = epollUpdateFd(fd, rg);
			if (r == 1) return;
				removeTimeout(rg->tmPos[wf]);
			if (r == 0) {
				updateTimeout(rg, timeout, wf);
				return;
			}
			e = errno;
		}
		throw SystemException(e);
	} else {
		FdReg *rg = addReg(fd);
		rg->cb[wf] = fn;
		updateTimeout(rg, timeout, wf);
	}
}

void EPollAsync::cancelWait(WaitFor wf, unsigned int fd) {
	Sync _(regLock);
	FdReg *rg = findReg(fd);
	if (rg != nullptr) {
		removeTimeout(rg->tmPos[wf]);
		rg->cb[wf] == nullptr;
		epollUpdateFd(fd,rg);
	}
}


void EPollAsync::run() {
	workerLock.lock();
	//callbacks are never called directly
	//all calls are scheduled here and executed at the end
	//where the worker lock is unlocked;
	CBList cbList;
	while (!stopped) {
		unsigned int tm = calcTimeout();
		struct epoll_event ev[32];
		int r = epoll_wait(epollfd,ev,32,tm);
		if (r == 0) {
			Sync _(regLock);
			onTimeout(cbList);
		} else {
			Sync _(regLock);
			for (int i = 0; i < r; i++) {
				if (ev[i].data.fd != 0) {
					onEvent(ev[i].events, ev[i].data.fd, cbList);
				} else {
					clearNotify();
				}
			}
		}
		workerLock.unlock();
		for (auto c : cbList) {
			runNoExcept(c.first, c.second);
		}
		cbList.clear();
		workerLock.lock();
	}
	workerLock.unlock();
}

void EPollAsync::stop() {
	stopped = true;
	sendNotify();
}

EPollAsync::~EPollAsync() {
	::close(epollfd);
}




}

