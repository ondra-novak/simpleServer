#include <fcntl.h>
#include <cstring>
#include <mutex>
#include "tcpStreamFactory.h"

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "../raii.h"


#include "../exceptions.h"
#include "async.h"
#include "localAddr.h"

#include "tcpStream.h"
#include "localAddr.h"
#include <csignal>

using ondra_shared::Handle;

namespace simpleServer {


TCPStreamFactory::TCPStreamFactory(NetAddr target,int ioTimeout)
	:target(target),ioTimeout(ioTimeout),stopped(false) {

	//Ignore SIGPIPE as it is best and very compatible option to handle sending errors

	signal(SIGPIPE, SIG_IGN);

}


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





static SocketObject connectSocket(const NetAddr &addr) {

	SocketObject s = addr.connect();
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

TCPConnect::TCPConnect(NetAddr target, int connectTimeout, int ioTimeout)
	:TCPStreamFactory(target, ioTimeout),connectTimeout(connectTimeout) {
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
			connAdr = t;

			auto a = t.getNextAddr();
			isNext = a != nullptr;
			t = a;

			errno = 0;
			auto s = connectSocket(connAdr);


			int e = errno;
			if (e != 0 && e != EWOULDBLOCK && e != EINTR && e != EAGAIN && e != EINPROGRESS) {
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

static SocketObject listenSocket(const NetAddr &addr) {
	SocketObject s = addr.listen();
	return s;
}


TCPListen::TCPListen(NetAddr source, int listenTimeout, int ioTimeout):TCPStreamFactory(source, ioTimeout),listenTimeout(listenTimeout) {
	NetAddr t = source;
	bool hasNext = true;
	do {
		openSockets.push_back(listenSocket(t));
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


/*
static void fillPollFdListen(int s, pollfd &fd) {
	fd.events = POLLIN;
	fd.fd = s;
	fd.revents = 0;
}

*/

static Stream acceptConnect(int s, int iotimeout) {
	unsigned char buff[256];
	struct sockaddr *sa = reinterpret_cast<struct sockaddr *>(buff);
	socklen_t slen = sizeof(buff);
	//non blocking because multiple threads can try to claim the socket
	int a = accept4(s, sa, &slen, SOCK_NONBLOCK|SOCK_CLOEXEC);
	if (a > 0) {
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

TCPListen::~TCPListen() noexcept {
	asyncData = nullptr;
	openSockets.clear();
}



struct ConnectShared {
	IStreamFactory::Callback cb;
	std::atomic<bool> finished;
	std::atomic<int> pending;
	std::exception_ptr cError;
	int timeout;
	int iotimeout;


	ConnectShared(IStreamFactory::Callback cb, int timeout,int iotimeout):cb(cb),finished(false),pending(0),timeout(timeout),iotimeout(iotimeout) {}

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

	//receive this addr
	NetAddr thisAddr(addr);
	AsyncProvider p(provider);

	errno = 0;

	SocketObject sock = connectSocket(addr);
	//if not wouldblock
	int e = errno;
	//create socket
	std::shared_ptr<SocketObject> s(new SocketObject(std::move(sock)));

	if (e != EINPROGRESS && e != EWOULDBLOCK && e != EAGAIN && e != 0) {
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
}


void TCPConnect::createAsync(const AsyncProvider &provider, const Callback &cb) {
	std::shared_ptr<ConnectShared> shared(new ConnectShared(cb, connectTimeout, ioTimeout));
	connectAsyncCycle(provider,target,shared);
}


class TCPListen::AsyncData: public RefCntObj {
public:
	AsyncData(TCPListen &owner):owner(owner),idleSockets(owner.openSockets.begin(), owner.openSockets.end()) {
	}


	void onSignal(int socket, AsyncState state) {
		Callback cb;
		AsyncProvider p;
		int iot, lst;
		bool stpd;
		{
			std::lock_guard<std::mutex> _(lock);
			std::swap(cb,curCallback);
			std::swap(p,curProvider);
			iot = iotimeout;
			lst = listenTimeout;
			stpd = stopped;
			idleSockets.push_back(socket);
		}
		if (cb!=nullptr) {
			if (state == asyncOK) {
				try {
					Stream sx = acceptConnect(socket, iot);
					if (sx == nullptr) {
						charge(p,cb,lst,iot);
					} else {
						sx.setAsyncProvider(p);
						cb(state,sx);
					}
				} catch (SystemException &e) {
					if (stpd || e.getErrNo() == EINVAL)
						cb(asyncEOF,nullptr);
					else
						cb(asyncError, nullptr);
				} catch (...) {
					cb(stpd?asyncEOF:asyncError, nullptr);
				}
			} else {
				cb(state, nullptr);
			}
		}
	}


	void charge(const AsyncProvider &p, const Callback &cb, int listenTimeout, int iotimeout) {
		std::lock_guard<std::mutex> _(lock);

		RefCntPtr<AsyncData> me(this);
		curCallback = cb;
		curProvider = p;

		for (int s: idleSockets) {

			auto fn = [me, s](AsyncState state){
				me->onSignal(s, state);
			};

			p.runAsync(AsyncResource(s, POLLIN), listenTimeout, fn);
		}
		idleSockets.clear();
		this->iotimeout = iotimeout;
		this->listenTimeout = listenTimeout;
	}

	void setStopped() {
		stopped = true;
	}

protected:
	TCPListen &owner;
	std::vector<int> idleSockets;
	Callback curCallback;
	AsyncProvider curProvider;
	int iotimeout;
	int listenTimeout;
	bool stopped = false;
	std::mutex lock;

};

void TCPListen::createAsync(const AsyncProvider &provider, const Callback &cb) {
	RefCntPtr<TCPListen> me(this);
	AsyncProvider p(provider);
	Callback ccb(cb);

	if (asyncData == nullptr) {
		asyncData = new AsyncData(*this);
	}

	asyncData->charge(provider, cb, listenTimeout,ioTimeout);

}

void TCPListen::stop() {
	TCPStreamFactory::stop();
	if (asyncData!= nullptr) {
		asyncData->setStopped();
		asyncData = nullptr;
	}
	for (auto &&f: openSockets) {
		shutdown(f, SHUT_RD);
	}
}




}

