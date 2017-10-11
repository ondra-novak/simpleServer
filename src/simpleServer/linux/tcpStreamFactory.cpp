#include <cstring>
#include <mutex>
#include "tcpStreamFactory.h"

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>



#include "../exceptions.h"


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

static int connectSocket(const NetAddr &addr, int &r) {
	BinaryView b = addr.toSockAddr();
	const struct sockaddr *sa = reinterpret_cast<const struct sockaddr *>(b.data);
	int s = socket(sa->sa_family, SOCK_STREAM, IPPROTO_TCP);
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
			int s = connectSocket(t,r);

			auto a = t.getNextAddr();
			isNext = a != nullptr;
			t = a;

			if (r == 0) {
				selectedSocket = s;
			} else if (r == -1) {
				int e = errno;
				if (e != EWOULDBLOCK && e != EINTR && e != EAGAIN && e != EINPROGRESS) {
					throw SystemException(e,"Error connecting socket");
				}
				pollfd fd;
				fd.fd = s;
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
				unsigned char buff[256];
				struct sockaddr *sa = reinterpret_cast<struct sockaddr *>(buff);
				socklen_t slen = sizeof(buff);
				//non blocking because multiple threads can try to claim the socket
				int a = accept4(s, sa, &slen, SOCK_NONBLOCK|SOCK_CLOEXEC);
				if (a > 0) {
					disableNagle(a);
					NetAddr addr = NetAddr::create(BinaryView(reinterpret_cast<const unsigned char *>(&sa), slen));
					return new TCPStream(a,ioTimeout, addr);
				} else if (stopped) {
					return Stream(nullptr);
				} else {
					int e =errno;
					if (e != EINTR && e != EAGAIN && e != EWOULDBLOCK)
						throw SystemException(e, "Failed to accept socket");
					//in case EWOULDBOCK - back to the waiting
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

}
