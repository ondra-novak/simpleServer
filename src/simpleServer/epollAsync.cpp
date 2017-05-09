
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include "epollAsync.h"

#include "exceptions.h"


namespace simpleServer {


EPollAsync::EPollAsync() {
	epollfd = epoll_create1(EPOLL_CLOEXEC);

	stopped = false;

}

void EPollAsync::run() {

/*
	while (!stopped) {

		TmReg tm = popTimeout();
		unsigned int wdelay;
		if (tm.fd == 0) {
			wdelay = -1;
		} else {
			TimePt now = std::chrono::steady_clock::now();
			if (tm.time < now) wdelay = 0;
			else wdelay = std::chrono::duration_cast<std::chrono::milliseconds>(tm.time - now).count();
		}
		struct epoll_event ev;
		int r = epoll_wait(epollfd,&ev,1,wdelay);
		if (r == -1) throw SystemException(errno);
		if (r == 0) {
			onTimeout(tm);
		} else {
			pushTimeout(tm);
			if (ev.data.fd != 0) {
				onEvent(ev.events, ev.data.fd);
			} else{

			}
		}
	}

*/
}

void EPollAsync::stop() {

}

EPollAsync::~EPollAsync() {
	::close(epollfd);
}

void EPollAsync::asyncWait(WaitFor wf, unsigned int fd, unsigned int timeout, CallbackFn fn) {
}



}

