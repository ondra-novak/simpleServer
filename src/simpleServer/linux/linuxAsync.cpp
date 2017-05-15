#include "linuxAsync.h"

#include <poll.h>
#include <sys/socket.h>

#include "../exceptions.h"

namespace simpleServer {


void LinuxAsync::checkSocketError(int fd) {
	int e = EFAULT;
	socklen_t l = sizeof(e);
	getsockopt(fd,SOL_SOCKET, SO_ERROR, &e, &l);
	if (e) throw SystemException(e);
}


class FakeSync: public LinuxAsync {
public:

	virtual void asyncWait(WaitFor wf, unsigned int fd, unsigned int timeout, CallbackFn fn) override;
	virtual bool cancelWait(WaitFor wf, unsigned int fd) override;
	virtual void run() override {}
	virtual void run(CallbackExecutor executor) {}
	virtual void stop() override {}

	FakeSync(unsigned int maxTimeout):maxTimeout(maxTimeout) {}


	unsigned int maxTimeout;

	int pollWait(WaitFor w, unsigned int fd, unsigned int timeout);
};



AsyncDispatcher AsyncDispatcher::createSync(unsigned int maxTimeout) {
	return AsyncDispatcher(PAsyncControl(new FakeSync(maxTimeout)));

}

int FakeSync::pollWait(WaitFor wf, unsigned int fd, unsigned int timeout) {

	unsigned int tm;
	bool fatalTm = timeout > maxTimeout;
	if (fatalTm) tm = maxTimeout;else tm = timeout;
	struct pollfd pfd;
	pfd.events = wf==wfRead?POLLIN:POLLOUT;
	pfd.fd = fd;
	int r = poll(&pfd,1,tm);
	if (r == -1) throw SystemException(errno);
	if (r == 0 && fatalTm) throw TimeoutException();
	if (pfd.events & POLLERR) return -1;
	return r;

}


void FakeSync::asyncWait(WaitFor wf, unsigned int fd, unsigned int timeout, CallbackFn fn) {

	int r = pollWait(wf,fd,timeout);
	if (r == -1) {
		try {
			LinuxAsync::checkSocketError(fd);
		} catch (...) {
			fn(etError);
		}
	} else if (r == 0) {
		fn(etTimeout);
	} else {
		fn(wf==wfRead?etReadEvent:etWriteEvent);
	}
}

bool FakeSync::cancelWait(WaitFor wf, unsigned int fd) {
	return false;
}



}
