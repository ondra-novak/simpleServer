#include <cstring>
#include <mutex>
#include "tcpStreamFactory.h"

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "../common/raii.h"


#include "../exceptions.h"
#include "async.h"


#include "tcpStream.h"

namespace simpleServer {


NetAddr TCPStreamFactory::getLocalAddress(const StreamFactory& sf) {

	const TCPStreamFactory &f = dynamic_cast<const TCPStreamFactory &>(*sf);
	return f.getLocalAddress();

}

NetAddr TCPStreamFactory::getPeerAddress(const Stream& stream) {
	const TCPStream &f = dynamic_cast<const TCPStream &>(*stream);
	return f.getPeerAddr();
}


StreamFactory TCPConnect::create(NetAddr target,
		int connectTimeout, int ioTimeout) {

	return new TCPConnect(target, connectTimeout, ioTimeout);

}


static void disableNagle(int socket) {
	int flag = 1;
	int result = setsockopt(socket,            /* socket affected */
	                        IPPROTO_TCP,     /* set option at TCP level */
	                        TCP_NODELAY,     /* name of option */
	                        (char *) &flag,  /* the cast is historical cruft */
	                        sizeof(int));    /* length of option value */
	 if (result < 0) {
		 int e = errno;
		 throw SystemException(e, "Unable to setup socket (TCP_NODELAY)");
	 }

}


typedef RAII<int, decltype(&::close), &::close> RAIISocket;

static RAIISocket connectSocket(const NetAddr &addr, int &r) {
	BinaryView b = addr.toSockAddr();
	const struct sockaddr *sa = reinterpret_cast<const struct sockaddr *>(b.data);
	RAIISocket s(socket(sa->sa_family, SOCK_STREAM, IPPROTO_TCP));
	if (s == -1) throw SystemException(errno);
	int nblock = 1;ioctl(s, FIONBIO, &nblock);
	disableNagle(s);
	r = ::connect(s, sa, b.length);
	return s;
}


static int waitForSockets(std::vector<pollfd> &sockets, int timeout) {

	int selectedSocket = 0;
	int e;
	int r;

	do {
		r = poll(sockets.data(), sockets.size(), timeout);
		if (r == -1) {
			e = errno;
			if (e != EAGAIN && e != EINTR) {
				throw SystemException(e,"Error waiting for connection");
			}
		} else if (r > 0) {
			for (auto iter = sockets.begin(); iter != sockets.end(); ++iter) {
				const pollfd &fd = *iter;
				socklen_t l = sizeof(e);
				getsockopt(fd.fd,SOL_SOCKET, SO_ERROR, &e, &l);
				if (e) {
					close(fd.fd);
					sockets.erase(iter);
					if (sockets.empty()) throw SystemException(e,"Failed to connect");

				} else {
					selectedSocket = fd.fd;
				}
			}
		}
	} while (r == 0 && selectedSocket ==0);
	return selectedSocket;


}

TCPConnect::TCPConnect(NetAddr target, int connectTimeout, int ioTimeout):TCPStreamFactory(target, ioTimeout) {
}

Stream TCPConnect::create() {
	if (stopped) return nullptr;

	std::vector<pollfd> sockets;
	int selectedSocket = 0;

	try {
		NetAddr t = target;
		NetAddr connAdr = t;
		bool isNext;

		do {
			int r;
			connAdr = t;

			auto a = t.getNextAddr();
			isNext = a != nullptr;
			t = a;

			auto s = connectSocket(connAdr,r);


			if (r == 0) {
				selectedSocket = s.detach();
			} else if (r == -1) {
				int e = errno;
				if (e != EWOULDBLOCK && e != EINTR && e != EAGAIN && e != EINPROGRESS) {
					throw SystemException(e,"Error connecting socket");
				}
				pollfd fd;
				fd.fd = s.detach();
				fd.events = POLLOUT;
				fd.revents = 0;
				sockets.push_back(fd);

				try {
					selectedSocket = waitForSockets(sockets, 1000);
				} catch (...) {
					if (!isNext) throw;
				}

			}



		} while (isNext && selectedSocket == 0);
		if (selectedSocket == 0) {
			selectedSocket = waitForSockets(sockets, connectTimeout);
			if (selectedSocket == 0) throw TimeoutException();
		}
		for (auto &&x:sockets) {
			if (x.fd != selectedSocket) close(x.fd);
		}

		return new TCPStream(selectedSocket, ioTimeout,connAdr);

	} catch (...) {
		for (auto &&x:sockets) {
			close(x.fd);
		}
		throw;
	}


}

Stream tcpConnect(NetAddr target, int connectTimeout, int ioTimeout) {
	auto factory = TCPConnect::create(target,connectTimeout,ioTimeout);
	return factory->create();
}

Stream tcpListen(NetAddr target, int listenTimeout, int ioTimeout) {
	StreamFactory f = TCPListen::create(target,listenTimeout,ioTimeout);
	return f->create();
}

StreamFactory TCPListen::create(NetAddr source, int listenTimeout,
		int ioTimeout) {

	return new TCPListen(source, listenTimeout, ioTimeout);

}

StreamFactory TCPListen::create(bool localhost, unsigned int port,
		int listenTimeout, int ioTimeout) {

	return new TCPListen(localhost, port, listenTimeout, ioTimeout);

}

static int listenSocket(const NetAddr &addr) {
	BinaryView b = addr.toSockAddr();
	const struct sockaddr *sa = reinterpret_cast<const struct sockaddr *>(b.data);
	int s = socket(sa->sa_family, SOCK_STREAM, IPPROTO_TCP);
	if (s == -1) throw SystemException(errno,"socket() failure");
	int nblock = 1;ioctl(s, FIONBIO, &nblock);
	if (::bind(s, sa, b.length) == -1) {
		int e = errno;
		close(s);
		throw SystemException(e,"Cannot bind socket to port");
	}
	if (::listen(s,SOMAXCONN) == -1) {
		int e = errno;
		close(s);
		throw SystemException(e,"Cannot activate listen mode on the socket");
	}
	return s;
}


TCPListen::TCPListen(NetAddr source, int listenTimeout, int ioTimeout):TCPStreamFactory(source, ioTimeout),listenTimeout(listenTimeout) {
	NetAddr t = source;
	bool hasNext = true;
	do {

		int s = listenSocket(t);
		openSockets.push_back(s);
		auto a = t.getNextAddr();
		hasNext = a != nullptr;
		t = a;
	} while (hasNext);

	unsigned char buff[256];
	socklen_t size = sizeof(buff);
	getsockname(openSockets[0],reinterpret_cast<struct sockaddr *>(buff),&size);
	target = NetAddr::create(BinaryView(buff, size));
	for (std::size_t i = 1; i < openSockets.size(); i++) {
		socklen_t size = sizeof(buff);
		getsockname(openSockets[i],reinterpret_cast<struct sockaddr *>(buff),&size);
		NetAddr x = NetAddr::create(BinaryView(buff, size));
		target = target + x;
	}


}

static NetAddr createListeningAddr(bool localhost, unsigned int port) {

	struct sockaddr_in sin4;
	std::memset(&sin4, 0,sizeof(sin4));
	sin4.sin_family = AF_INET;
	sin4.sin_port = htons(port);
	sin4.sin_addr.s_addr = htonl(localhost?INADDR_LOOPBACK:INADDR_ANY);
	NetAddr a4 = NetAddr::create(BinaryView(StringView<struct sockaddr_in>(&sin4,1)));



	struct sockaddr_in6 sin6;
	std::memset(&sin6, 0,sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	sin6.sin6_port = htons(port);
	sin6.sin6_addr = localhost?in6addr_loopback:in6addr_any;
	NetAddr a6 = NetAddr::create(BinaryView(StringView<struct sockaddr_in6>(&sin6,1)));

	return a4+a6;

}


TCPListen::TCPListen(bool localhost, unsigned int port, int listenTimeout, int ioTimeout)
	:TCPListen(createListeningAddr(localhost,port),listenTimeout, ioTimeout) {}

static void fillPollFdListen(int s, pollfd &fd) {
	fd.events = POLLIN;
	fd.fd = s;
	fd.revents = 0;
}



static Stream acceptConnect(int s, int iotimeout) {
	unsigned char buff[256];
	struct sockaddr *sa = reinterpret_cast<struct sockaddr *>(buff);
	socklen_t slen = sizeof(buff);
	//non blocking because multiple threads can try to claim the socket
	int a = accept4(s, sa, &slen, SOCK_NONBLOCK|SOCK_CLOEXEC);
	if (a > 0) {
		disableNagle(a);
		NetAddr addr = NetAddr::create(BinaryView(reinterpret_cast<const unsigned char *>(&sa), slen));
		return new TCPStream(a,iotimeout, addr);
	} else {
		int e =errno;
		if (e != EINTR && e != EAGAIN && e != EWOULDBLOCK)
			throw SystemException(e, "Failed to accept socket");
		return nullptr;
	}

}

Stream TCPListen::create() {


	//this code can be called MT recursive

	if (stopped) return Stream(nullptr);

	auto scount = openSockets.size();

	//so we need to create state on stack

	pollfd fds[scount];
	for (decltype(scount) i = 0; i <scount; i++) {
		auto &&fd = fds[i];
		fd.fd = openSockets[i];
		fd.events = POLLIN;
		fd.revents = 0;
	}

	int r;
	do {
		r = poll(fds,scount,listenTimeout);
		if (stopped) return Stream(nullptr);
		if (r == 0) {
			return Stream(nullptr);
		} else if (r < 0) {
			int e = errno;
			if (e != EINTR && e != EAGAIN && e != EWOULDBLOCK) throw SystemException(e, "Failed to wait on listening socket(s)");
		} else {
			for (auto &&x: fds) if (x.revents) {
				int s = x.fd;
				x.revents = 0;
				try {
					return acceptConnect(s,ioTimeout);
				} catch (...) {
					if (stopped) return nullptr;
					throw;
				}
			}
		}

	} while (true);

}

TCPListen::~TCPListen() {
	for(auto &&x:openSockets)close(x);
}


void TCPListen::stop() {
	TCPStreamFactory::stop();
	for (auto &&f: openSockets) {
		shutdown(f, SHUT_RD);
	}
}

struct ConnectShared {
	IStreamFactory::Callback cb;
	std::atomic<bool> finished;
	std::atomic<int> pending;
	std::exception_ptr cError;
	int timeout;
	int iotimeout;


	ConnectShared(IStreamFactory::Callback cb, int timeout,int iotimeout):cb(cb),finished(false),timeout(timeout),iotimeout(iotimeout) {}

	void inc_pending() {
		++pending;
	}
	void dec_pending() {
		if (--pending == 0) {
			if (!((bool)finished)) {
				if (cError != nullptr) {
					try {
						std::rethrow_exception(cError);
					} catch (...) {
						cb(asyncError, nullptr);
					}
				} else {
					cb(asyncTimeout,nullptr);
				}
			}
		}
	}
};



static void connectAsyncCycle(const AsyncProvider& provider, const NetAddr &addr, std::shared_ptr<ConnectShared> shared) {

	int r;
	//receive this addr
	NetAddr thisAddr(addr);
	AsyncProvider p(provider);

	int sock = connectSocket(addr,r);
	//if error in connect
	if (r) {
		//if not wouldblock
		int e = errno;
		//create socket
		std::shared_ptr<RAIISocket> s(new RAIISocket(sock));

		if (e != EINPROGRESS && e != EWOULDBLOCK && e != EAGAIN) {
			//throw exception
			throw SystemException(e, "Connect failed");
		}

		auto fnLong = [=](AsyncState st) {
			//in case of OK
			if (st == asyncOK) {
				bool exp = false;
				//check whether still waiting for connection
				if (shared->finished.compare_exchange_strong(exp,true)) {
					//if yes, create stream
					Stream sx = new TCPStream(s->detach(), shared->iotimeout, thisAddr);
					//give the result to the callback function
					sx.setAsyncProvider(provider);
					shared->cb(st, sx);
				}
				//otherwise nothing here
			} else if (st == asyncError) {
				shared->cError = std::current_exception();
			}
			shared->dec_pending();
		};
		//declare callback
		auto fnShort = [=](AsyncState st) {
			//in case of OK
			if (st == asyncOK) {
				fnLong(st);
			} else  {
				//connect has longPart and shortPart/
				//in case of shortPart, create connection to next address
				auto nx = thisAddr.getNextAddr();
				//cycle addresses
				while (nx != nullptr) {
					try {
						//recursively call connect for next address (in shortPart)
						connectAsyncCycle(p,nx,shared);
						//break
						break;
					} catch (...) {
						//if exception, store it
						shared->cError = std::current_exception();
						//get next address
						nx = NetAddr(nx).getNextAddr();
					}
				}
				if (st == asyncTimeout) {
					shared->inc_pending();
					p->runAsync(AsyncResource(*s, POLLOUT), shared->timeout, fnLong);
				}
			}
			shared->dec_pending();
		};

		shared->inc_pending();
		p->runAsync(AsyncResource(*s,POLLOUT), 1000, fnShort);
	} else {
		bool exp = false;
		//create socket
		std::shared_ptr<RAIISocket> s(new RAIISocket(sock));

		if (shared->finished.compare_exchange_strong(exp,true)) {
			Stream sx = new TCPStream(s->detach(), shared->timeout, thisAddr);
			sx.setAsyncProvider(provider);
			shared->cb(asyncOK, sx);
		}
	}
}


void TCPConnect::createAsync(const AsyncProvider &provider, const Callback &cb) {
	std::shared_ptr<ConnectShared> shared(new ConnectShared(cb, connectTimeout, ioTimeout));
	connectAsyncCycle(provider,target,shared);
}

void TCPListen::createAsync(const AsyncProvider &provider, const Callback &cb) {
	cbrace = false;
	RefCntPtr<TCPListen> me(this);
	AsyncProvider p(provider);
	Callback ccb(cb);

	for (int s: openSockets) {

		auto fn = [me, s, p, ccb](AsyncState st){
			bool exp = false;
			if (me->cbrace.compare_exchange_strong(exp, true)) {
				if (st == asyncOK) {
					try {
						Stream sx = acceptConnect(s,me->ioTimeout);
						if (sx == nullptr) {
							me->createAsync(p, ccb);
						} else {
							sx.setAsyncProvider(p);
							ccb(st, sx);
						}

					} catch (...) {
						ccb(asyncError,nullptr);
					}
				} else {
					ccb(st, nullptr);
				}
			}
		};

		provider->runAsync(AsyncResource(s, POLLIN), listenTimeout, fn);
	}
}

}
