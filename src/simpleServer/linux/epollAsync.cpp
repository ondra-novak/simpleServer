
#include <unistd.h>
#include <fcntl.h>
#include "epollAsync.h"

#include "../mt.h"
#include "../connection.h"

#include "../exceptions.h"


namespace simpleServer {


AsyncControl AsyncControl::create() {
	return AsyncControl(new EPollAsync);

}


EPollAsync::EPollAsync() {

	stopped = false;

	pipe2(notifyPipe, O_CLOEXEC|O_NONBLOCK);
	epoll.add(notifyPipe[0],EPoll::input);

}

void EPollAsync::updateTimeout(FdReg* rg, unsigned int timeout, WaitFor wf) {
	if (rg->tmPos[wf])
		removeTimeout(rg->tmPos[wf]);
	if (timeout != infinity) {
		addTimeout(TmReg(Clock::now() + Millis(timeout),rg, wf));
	}
}

EPollAsync::FdReg* EPollAsync::findReg(unsigned int fd) {
	if (fd >= fdRegMap.size()) fdRegMap.resize(fd+1);
	return &fdRegMap[0]+fd;
}

EPollAsync::FdReg* EPollAsync::addReg(unsigned int fd) {
	FdReg* r = findReg(fd);
	r->used = true;
	r->fd = fd;
}

void EPollAsync::removeTimeout(TmReg* &reg) {
	std::size_t pos = reg - &tmoutQueue.top();
	tmoutQueue.remove_at(pos);
	reg = nullptr;
}

void EPollAsync::addTimeout(TmReg&& reg) {
	tmoutQueue.push(std::move(reg));
}

bool EPollAsync::epollUpdateFd(FdReg* rg) {
	int events = 0;
	bool noev = true;
	if (rg->cb[wfRead] != nullptr) {
		events |= EPoll::input;
		noev = false;
	}

	if (rg->cb[wfRead] != nullptr) {
		events |= EPoll::output;
		noev = false;
	}

	if (noev) {
		epoll.remove(rg->fd);
		rg->fd = 0;
		rg->used = false;
		return false;
	} else {
		epoll.update(rg->fd,events);
		return true;
	}
}

void EPollAsync::asyncWait(WaitFor wf, unsigned int fd, unsigned int timeout, CallbackFn fn) {
	Sync _(regLock);
	FdReg *rg;

	if (!epoll.add(fd, wf == wfRead?EPoll::input:EPoll::output)) {
		rg = findReg(fd);
	} else {
		rg = addReg(fd);
	}
	rg->cb[wf] = fn;
	if (epollUpdateFd(rg))
		updateTimeout(rg, timeout, wf);
	else
		removeTimeout(rg->tmPos[wf]);
}

bool EPollAsync::cancelWait(WaitFor wf, unsigned int fd) {
	Sync _(regLock);
	FdReg *rg = findReg(fd);
	bool res = false;
	if (rg != nullptr) {
		if (rg->cb[wf]) {
			removeTimeout(rg->tmPos[wf]);
			rg->cb[wf] == nullptr;
			epollUpdateFd(rg);
			res = true;
		}
	}
	return res;
}


void EPollAsync::run(CallbackExecutor executor) {
	workerLock.lock();
	//callbacks are never called directly
	//all calls are scheduled here and executed at the end
	//where the worker lock is unlocked;
	CBList cbList;

	while (!stopped) {
		unsigned int tm = calcTimeout();
		EPoll::Events ev = epoll.getNext(tm);
		if (ev.isTimeout()) {
			Sync _(regLock);
			onTimeout(cbList);
		} else {
			Sync _(regLock);
			onEvent(ev, cbList);
		}

		workerLock.unlock();

		if (executor==nullptr) {
			for (auto c : cbList) {
				runNoExcept(c.first, c.second);
			}
		} else{
			for (auto c : cbList) {
				executor([c](){c.first(c.second);});
			}
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


unsigned int EPollAsync::calcTimeout() {
	if (tmoutQueue.empty()) return -1;
	TimePt pt = Clock::now();
	const TmReg &reg = tmoutQueue.top();
	if (reg.time < pt) return 0;
	return std::chrono::duration_cast<std::chrono::milliseconds>(reg.time - pt).count();
}

void EPollAsync::onTimeout(CBList& cblist) {
	TimePt pt = Clock::now();
	do {
		const TmReg &z = tmoutQueue.top();
		FdReg *reg = z.fdreg;
		WaitFor wf = z.wf;
		if (z.time > pt) break;
		cblist.push_back(CallbackWithArg(std::move(reg->cb[wf]), etTimeout));
		tmoutQueue.pop();
		epollUpdateFd(reg);
	}
	while (true);
}

void EPollAsync::onEvent(const EPoll::Events &ev, CBList& cblist) {
	FdReg *reg = findReg(ev.data.fd);
	if (ev.isError()) {
		if (reg->cb[wfRead] != nullptr)
			cblist.push_back(CallbackWithArg(std::move(reg->cb[wfRead]), etError));
		if (reg->cb[wfWrite] != nullptr)
			cblist.push_back(CallbackWithArg(std::move(reg->cb[wfWrite]), etError));
	}
	if (ev.isInput()) {
		if (reg->cb[wfRead] != nullptr)
			cblist.push_back(CallbackWithArg(std::move(reg->cb[wfRead]), etReadEvent));
	}
	if (ev.isOutput()) {
		if (reg->cb[wfWrite] != nullptr)
			cblist.push_back(CallbackWithArg(std::move(reg->cb[wfWrite]), etWriteEvent));
	}
	epollUpdateFd(reg);
}

void EPollAsync::clearNotify() {
	char buff[20];
	(void)::read(notifyPipe[0], buff, 20);
}

void EPollAsync::sendNotify() {
	char c = 1;
	int r = ::write(notifyPipe[1],&c,1);
	if (r==-1) throw SystemException(errno);
}

}

