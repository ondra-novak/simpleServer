/*
 * socketConnection.cpp
 *
 *  Created on: May 6, 2017
 *      Author: ondra
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include "linuxAsync.h"

#include <poll.h>
#include <cstring>
#include "socketConnection.h"

#include <netdb.h>
#include <unistd.h>


#include "../exceptions.h"

namespace simpleServer {

SocketConnection::SocketConnection(int sock,unsigned int iotimeout, NetAddr peerAddr)
:peerAddr(peerAddr)
,sock(sock),iotimeout(iotimeout)
,rdbuff_used(0)
,rdbuff_pos(0)
,wrbuff_pos(0)
,eof(false)
,eofReported(false)
{
}

void SocketConnection::waitForData() {
	struct pollfd fdinfo;
	fdinfo.events = POLLIN;
	fdinfo.fd = sock;
	fdinfo.revents = 0;
	int res = poll(&fdinfo,1,iotimeout);
	if (res == 0) throw TimeoutException();
	if (res == -1) {
		int e = errno;
		if (e == EINTR) waitForData();
		else throw SystemException(errno);
	}
}

void SocketConnection::waitForSend() {
	struct pollfd fdinfo;
	fdinfo.events = POLLOUT;
	fdinfo.fd = sock;
	fdinfo.revents = 0;
	int res = poll(&fdinfo,1,iotimeout);
	if (res == 0) throw TimeoutException();
	if (res == -1) {
		int e = errno;
		if (e == EINTR) waitForSend();
		throw SystemException(errno);
	}
}

bool SocketConnection::isClosed() const {
	return eof;
}

void SocketConnection::sendAll(BinaryView data) {
	while (data.length) {
		int r = send(sock, data.data, data.length, MSG_DONTWAIT);
		if (r == -1) {
			int e = errno;
			if (e != EWOULDBLOCK) throw SystemException(errno);
			waitForSend();
		} else {
			data = data.substr(r);
		}
	}
}
void SocketConnection::closeOutput() {
	flush();
	shutdown(sock,SHUT_WR);
}

static int connectSocket(const NetAddr &addr, int &r) {
	BinaryView b = addr.toSockAddr();
	const struct sockaddr *sa = reinterpret_cast<const struct sockaddr *>(b.data);
	int s = socket(sa->sa_family, SOCK_STREAM, IPPROTO_TCP);
	if (s == -1) throw SystemException(errno);
	int nblock = 1;ioctl(s, FIONBIO, &nblock);
	r = ::connect(s, sa, b.length);

}

static Connection socketToConnection(const ConnectParams &params, int sock, const NetAddr &addr) {
	if (params.factory == nullptr) {
		PSocketConnection conn = new SocketConnection(sock, infinity ,addr);
		return Connection(PConnection::staticCast(conn));
	} else {
		return params.factory(addr,sock);
	}
}

Connection Connection::connect(const NetAddr &addr, const ConnectParams &params) {
	int r;
	int s = connectSocket(addr,r);
	if (r == -1) {
		int e = errno;
		if (e == EWOULDBLOCK || e == EINPROGRESS) {

			struct pollfd pfd;
			pfd.events = POLLOUT;
			pfd.fd = s;
			pfd.revents = 0;
			e = poll(&pfd,1,params.waitTimeout);
			if (e < 0) {
				e = errno;
				::close(s);
				throw SystemException(e);
			} else if (e == 0) {
				::close(s);
				throw TimeoutException();
			} else {
				try {
					LinuxAsync::checkSocketError(s);
				} catch (...) {
					close(s);
					throw;
				}
			}

		} else {
			::close(s);
			throw SystemException(e);
		}
	}
	return socketToConnection(params,s,addr);
}

void Connection::connect(const NetAddr &addr, AsyncControl cntr, ConnectCallback callback,const ConnectParams &params) {
	int r;
	int sock = connectSocket(addr,r);
	if (r == -1) {
		int e = errno;
		if (e == EWOULDBLOCK || e == EINPROGRESS) {

			LinuxAsync &async = dynamic_cast<LinuxAsync &>(*cntr.getHandle());
			NetAddr a(addr);
			ConnectParams p(params);
			async.asyncWait(LinuxAsync::wfWrite,sock,params.waitTimeout,[a,sock,callback,p](LinuxAsync::EventType t){
				switch (t) {
				case LinuxAsync::etError: try {
					LinuxAsync::checkSocketError(sock);
				} catch (...) {
					callback(asyncError,nullptr);
				}
				break;
				case LinuxAsync::etTimeout:
					callback(asyncTimeout,nullptr);
				break;
				case LinuxAsync::etWriteEvent: {
					Connection c = socketToConnection(p,sock,a);
					callback(asyncOK,&c);
				}
				break;
				}
			});

		} else {
			::close(sock);
			throw SystemException(e);
		}

	} else {
		Connection c(socketToConnection(params,sock,addr));
		callback(asyncOK,&c);
	}
}

void SocketConnection::closeInput() {
	shutdown(sock,SHUT_RD);
}

SocketConnection::~SocketConnection() {
	::close(sock);
}

int SocketConnection::recvData(unsigned char* buffer, std::size_t size, bool nonblock) {
	int r = recv(sock, buffer, size, MSG_DONTWAIT);
	while (r == -1) {
		int e = errno;
		if (e != EWOULDBLOCK && e != EINTR) throw SystemException(e);
		if (nonblock) return -1;
		waitForData();
		r = recv(sock, buffer, size, MSG_DONTWAIT);
	}
	return r;
}

void SocketConnection::asyncRead(AsyncControl cntr, Callback callback,unsigned int timeoutOverride) {
	BinaryView b = getReadBuffer();
	if (b.empty()) {
		if (eof) try {
			callback(asyncOK, b);
		} catch (...) {
			callback(asyncError, BinaryView(nullptr,0));
		}
		LinuxAsync &async = dynamic_cast<LinuxAsync &>(*cntr.getHandle());
		RefCntPtr<SocketConnection> me(this);
		async.asyncWait(LinuxAsync::wfRead,sock,timeoutOverride?timeoutOverride:iotimeout,[me,cntr,callback,timeoutOverride](LinuxAsync::EventType ev){
			switch (ev) {
			case LinuxAsync::etError: try {
					LinuxAsync::checkSocketError(me->sock);
				} catch (...) {
					callback(asyncError,BinaryView(nullptr,0));
				}
				break;
			case LinuxAsync::etTimeout:
				callback(asyncTimeout, BinaryView(nullptr,0));
				break;
			case LinuxAsync::etReadEvent: try {
						int i = me->recvData(me->rdbuff, sizeof(me->rdbuff), true);
						if (i == -1) {
							me->asyncRead(cntr, callback, timeoutOverride);
						}
						if (i == 0) {
							me->eof = true;
							me->eofReported = true;
							callback(asyncEOF, BinaryView(nullptr,0));
						} else {
							me->rdbuff_pos = 0;
							me->rdbuff_used = i;
							callback(asyncOK, me->getReadBuffer());
						}
					}
				catch(...) {
					callback(asyncError,BinaryView(nullptr,0));
				}
				break;
			}
		});
	} else {
		callback(asyncOK,b);
	}
}

void SocketConnection::asyncWrite(BinaryView data,AsyncControl cntr, Callback callback,unsigned int timeoutOverride) {
	std::size_t remain = sizeof(wrbuff) - wrbuff_pos;
	if (remain > data.length) {
		writeData(data);
		callback(asyncOK,data);
	} else if (wrbuff_pos) {
		writeData(data.substr(0,remain-1));
		RefCntPtr<SocketConnection> me(this);
		asyncFlush(cntr, [me, data,remain, cntr, callback, timeoutOverride](AsyncState state, BinaryView b){
			if (state == asyncOK) {
				if (data.length - remain +1 < sizeof(me->rdbuff)) {
					me->writeData(data.substr(remain-1));
					callback(asyncOK,data);
				} else {
					me->runAsyncWrite(data,remain-1,cntr,callback,timeoutOverride);
				}
			} else {
				callback(state, data);
			}
		},timeoutOverride);
	} else {
		runAsyncWrite(data,0,cntr,callback,timeoutOverride);
	}
}

void SocketConnection::runAsyncWrite(BinaryView data,std::size_t offset,AsyncControl cntr, Callback callback,unsigned int timeoutOverride) {
		RefCntPtr<SocketConnection> me(this);
		LinuxAsync &async = dynamic_cast<LinuxAsync &>(*cntr.getHandle());
		async.asyncWait(LinuxAsync::wfWrite,sock,timeoutOverride?timeoutOverride:iotimeout,
				[me,data,offset,cntr,callback,timeoutOverride] (LinuxAsync::EventType ev) {
					switch (ev) {
					case LinuxAsync::etError:
						try {
							LinuxAsync::checkSocketError(me->sock);
						} catch (...) {
							callback(asyncError, data);
						}
						break;
					case LinuxAsync::etTimeout:
						callback(asyncTimeout, data);
						break;
					}
					int r = send(me->sock,data.data+offset,data.length-offset,MSG_DONTWAIT);
					if (r == -1) {
						int e;
						if (e == EWOULDBLOCK || e==EINTR) {
							me->runAsyncWrite(data,offset,cntr,callback,timeoutOverride);
						} else {
							try {
								throw SystemException(e);
							} catch (...) {
								callback(asyncError, data);
							}
						}
					} else {
						auto newOffset = offset + r;
						if (newOffset == data.length) {
							callback(asyncOK, data);
						} else {
							me->runAsyncWrite(data,newOffset,cntr,callback,timeoutOverride);
						}

					}
				});
	}


void SocketConnection::asyncFlush(AsyncControl cntr, Callback callback,	unsigned int timeoutOverride) {
	if (wrbuff_pos) {
		runAsyncWrite(BinaryView(wrbuff,wrbuff_pos), 0, cntr, callback, timeoutOverride);
		wrbuff_pos = 0;
	}
}



BinaryView SocketConnection::getReadBuffer() const {
	return BinaryView(rdbuff+rdbuff_pos, rdbuff_used - rdbuff_pos);
}

bool SocketConnection::cancelAsyncRead(AsyncControl cntr) {
	LinuxAsync &async = dynamic_cast<LinuxAsync &>(*cntr.getHandle());
	return async.cancelWait(LinuxAsync::wfRead,sock);

}
bool SocketConnection::cancelAsyncWrite(AsyncControl cntr){
	LinuxAsync &async = dynamic_cast<LinuxAsync &>(*cntr.getHandle());
	return async.cancelWait(LinuxAsync::wfWrite,sock);

}



} /* namespace simpleServer */
