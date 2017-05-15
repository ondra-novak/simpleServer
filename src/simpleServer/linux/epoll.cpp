#include <sys/epoll.h>
#include <unistd.h>

#include "epoll.h"

#include <cstring>


#include "../exceptions.h"

namespace simpleServer {

const unsigned int EPoll::input = EPOLLIN;
const unsigned int EPoll::output = EPOLLOUT;
const unsigned int EPoll::error = EPOLLERR;
const unsigned int EPoll::edgeTrigger = EPOLLET;
const unsigned int EPoll::oneShot = EPOLLONESHOT;

EPoll::EPoll() {
	fd = epoll_create1(EPOLL_CLOEXEC);
	if (fd == -1) {
		throw SystemException(errno);
	}
}

EPoll::~EPoll() {
	::close(fd);
}

bool EPoll::Events::isTimeout() const {
	return events == 0;
}

bool EPoll::Events::isInput() const {
	return events & input;
}

bool EPoll::Events::isOutput() const {
	return events & output;
}

bool EPoll::Events::isError() const {
	return events & error;
}

int EPoll::Events::getDataFd() const {
	return data.fd;
}

void* EPoll::Events::getDataPtr() const {
	return data.ptr;
}

uint32_t EPoll::Events::getData32() const {
	return data.u32;
}

uint64_t EPoll::Events::getData64() const {
	return data.u64;
}



EPoll::Events EPoll::getNext(unsigned int timeout) {
	if (events.empty()) {
		struct epoll_event buffer[32];
		int a = epoll_wait(fd,buffer,32,timeout);
		if (a == -1)
			throw SystemException(errno);
		if (a == 0)
			return Events();

		for (unsigned int i = 0; i < a; i++) {
			events.push(Events(buffer[i].events, &buffer[i].data));
		}
	}
	class Tmp {
	public:
		Tmp(std::queue<Events> &x):x(x) {}
		~Tmp() {x.pop();}

		std::queue<Events> &x;
	};

	Tmp tmp(events);
	return events.front();
}


static bool epollAdd(int epoll, int fd, struct epoll_event &ev) {
	if (epoll_ctl(epoll,EPOLL_CTL_ADD,fd,&ev) == -1) {
		int err = errno;
		if (err == EEXIST) return false;
		else throw SystemException(err);
	}
	return true;

}

static bool epollMod(int epoll, int fd, struct epoll_event &ev) {
	if (epoll_ctl(epoll,EPOLL_CTL_MOD,fd,&ev) == -1) {
		int err = errno;
		if (err == ENOENT) return false;
		else throw SystemException(err);
	}
	return true;

}

bool EPoll::add(int fd, unsigned int events, void* ptr) {
	struct epoll_event ev;
	ev.data.ptr = ptr;
	ev.events = events;
	return epollAdd(this->fd, fd, ev);
}

bool EPoll::add(int fd, unsigned int events) {
	struct epoll_event ev;
	ev.data.fd = fd;
	ev.events = events;
	return epollAdd(this->fd, fd, ev);
}

bool EPoll::add(int fd, unsigned int events, uint32_t u32) {
	struct epoll_event ev;
	ev.data.u32 = u32;
	ev.events = events;
	return epollAdd(this->fd, fd, ev);
}

bool EPoll::add(int fd, unsigned int events, uint64_t u64) {
	struct epoll_event ev;
	ev.data.u64 = u64;
	ev.events = events;
	return epollAdd(this->fd, fd, ev);
}

bool EPoll::update(int fd, unsigned int events, void* ptr) {
	struct epoll_event ev;
	ev.data.ptr = ptr;
	ev.events = events;
	return epollMod(this->fd, fd, ev);
}

bool EPoll::update(int fd, unsigned int events) {
	struct epoll_event ev;
	ev.data.fd = fd;
	ev.events = events;
	return epollMod(this->fd, fd, ev);
}

bool EPoll::update(int fd, unsigned int events, uint32_t u32) {
	struct epoll_event ev;
	ev.data.u32 = u32;
	ev.events = events;
	return epollMod(this->fd, fd, ev);
}

bool EPoll::update(int fd, unsigned int events, uint64_t u64) {
	struct epoll_event ev;
	ev.data.u64 = u64;
	ev.events = events;
	return epollMod(this->fd, fd, ev);
}

bool EPoll::remove(int fd) {
	struct epoll_event ev;
	if (epoll_ctl(this->fd,EPOLL_CTL_DEL,fd,&ev) == -1) {
		int e = errno;
		if (e == ENOENT) return false;
		throw SystemException(e);
	}
	return true;
}

EPoll::Events::Events(unsigned int events, void* data)
:events(events)
{
	std::memcpy(&this->data, data, sizeof(this->data));
}

}
