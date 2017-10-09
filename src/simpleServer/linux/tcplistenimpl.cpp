#include <netdb.h>
#include <unistd.h>
#include "tcplistenimpl.h"
#include "socketConnection.h"

#include <poll.h>

#include <cstring>

#include "linuxAsync.h"

#include "../exceptions.h"

namespace simpleServer {

TCPListener::TCPListener(NetAddr addr, const ConnectParams& params)
:owner(new TCPListenerImpl(addr,params))
{
}

TCPListener::TCPListener(unsigned int port, Range range,const ConnectParams& params)
:owner(new TCPListenerImpl(port,range,params)) {
}

TCPListener::TCPListener(Range range, unsigned int& port,const ConnectParams& params)
:owner(new TCPListenerImpl(range,port,params))
{
}

static int createSocket(const struct sockaddr *sa, std::size_t sa_len) {
	int s = socket(sa->sa_family,SOCK_STREAM,IPPROTO_TCP);
	int on = 1;
	if (s == -1) throw SystemException(errno);
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&on,sizeof(on)) < 0) {
		int e = errno;
		close(s);
		throw SystemException(e);
	}
	if (bind(s,sa,sa_len) || listen(s,16)) {
		int e = errno;
		close(s);
		throw SystemException(e);
	}
	return s;
}


TCPListenerImpl::TCPListenerImpl(NetAddr addr, const ConnectParams &params)
	:params(params)
{
	auto ssock = addr.toSockAddr();
	const struct sockaddr *sa = reinterpret_cast<const struct sockaddr *>(ssock.data);
	sock = createSocket(sa,ssock.length);
}

TCPListenerImpl::TCPListenerImpl(unsigned int port, Range range, const ConnectParams &params)
	:params(params)
{
	if (range == network6 || range == localhost6) {
		struct sockaddr_in6 addr;
		std::memset(&addr,0,sizeof(addr));
		if (range == localhost6) {
			addr.sin6_addr = in6addr_loopback;
		} else {
			addr.sin6_addr = in6addr_any;
		}
		addr.sin6_family = AF_INET6 ;
		addr.sin6_port = htons(port);
		sock = createSocket(reinterpret_cast<const struct sockaddr *>(&addr), sizeof(addr));
	} else {
		struct sockaddr_in addr;
		std::memset(&addr,0,sizeof(addr));
		if (range == localhost) {
			addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		} else {
			addr.sin_addr.s_addr = htonl(INADDR_ANY);
		}
		addr.sin_family = AF_INET ;
		addr.sin_port = htons(port);
		sock = createSocket(reinterpret_cast<const struct sockaddr *>(&addr), sizeof(addr));
	}
}

TCPListenerImpl::TCPListenerImpl(Range range, unsigned int& port, const ConnectParams &params)
	:TCPListenerImpl(0, range, params)
{
	struct sockaddr_in6 addr;
	socklen_t size = sizeof(addr);
	getsockname(sock,reinterpret_cast<struct sockaddr *>(&addr),&size);
	port = htons(addr.sin6_port);
}


Connection TCPListenerImpl::accept() {
	unsigned char buff[256];
	struct sockaddr *sa = reinterpret_cast<struct sockaddr *>(buff);
	socklen_t slen = sizeof(buff);
	int a = accept4(sock, sa, &slen, SOCK_NONBLOCK|SOCK_CLOEXEC);
	while (a == -1) {
		int e = errno;
		if (e == EINVAL) return Connection(nullptr);
		if (e != EAGAIN && e != EWOULDBLOCK && e != EINTR)
			throw SystemException(e);
		waitForConnection();
		a = accept4(sock, sa, &slen, SOCK_NONBLOCK|SOCK_CLOEXEC);
	}
	return createConnection(a,BinaryView(buff, slen));
}


void TCPListenerImpl::stop() {
	shutdown(sock,SHUT_RD);
}

void TCPListenerImpl::waitForConnection() {
	struct pollfd fdinfo;
	fdinfo.events = POLLIN;
	fdinfo.fd = sock;
	fdinfo.revents = 0;
	int res = poll(&fdinfo,1,params.waitTimeout);
	if (res == 0) throw TimeoutException();
	if (res == -1) throw SystemException(errno);
}

TCPListenerImpl::~TCPListenerImpl() {
	::close(sock);
}

Connection TCPListenerImpl::createConnection(int sock,const BinaryView& addrInfo) {
	NetAddr addr = NetAddr::create(addrInfo);
	if (params.factory != nullptr) return params.factory(addr, sock);
	else return Connection(PConnection::staticCast(
			PSocketConnection(new SocketConnection(sock,infinity,addr))));
}

/*
void TCPListenerImpl::asyncAccept(const AsyncDispatcher &cntr, AsyncCallback callback, unsigned int timeoutOverride) {

	//keep this object valid during async
	RefCntPtr<TCPListenerImpl> me (this);
	//extract epollasync
	LinuxAsync &async = dynamic_cast<LinuxAsync &>(*cntr.getHandle());
	//request async wait - for read, the socket, timeout, and callback
	async.asyncWait(LinuxAsync::wfRead, sock, timeoutOverride?timeoutOverride:params.waitTimeout,
			[callback,me,this](LinuxAsync::EventType ev){
				switch(ev) {
				case LinuxAsync::etError:
					try {
						LinuxAsync::checkSocketError(sock);
					} catch (...) {
						callback(asyncError, nullptr);
					}
					break;
				case LinuxAsync::etTimeout:
					callback(asyncTimeout,nullptr);
					break;
				case LinuxAsync::etReadEvent: {
					unsigned char buff[256];
					struct sockaddr *sa = reinterpret_cast<struct sockaddr *>(buff);
					socklen_t slen = sizeof(buff);
					//accept the connection
					int a = accept4(sock, sa, &slen, SOCK_NONBLOCK|SOCK_CLOEXEC);
					//on error
					if (a == -1) {
						int e = errno;
						if (e == EINVAL) callback(asyncEOF, nullptr);
						try {
							throw SystemException(e);
						} catch (...) {
							callback(asyncError,nullptr);
						}
					} else {
						//on success - create connection and call callback
						Connection c = createConnection(a,BinaryView(buff, slen));
						callback(asyncOK, &c);
					}
				}break;

				}
		});
}

*/
} /* namespace simpleServer */

