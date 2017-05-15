/*
 * socketConnection.cpp
 *
 *  Created on: May 6, 2017
 *      Author: ondra
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include "epollAsync.h"

#include <poll.h>
#include <cstring>
#include "socketConnection.h"

#include <netdb.h>
#include <unistd.h>


#include "exceptions.h"

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

/*
BinaryView SocketConnection::readData(unsigned int prevRead) {

	BinaryView buff = getReadBuffer();
	if (prevRead >= buff.length) {
		rdbuff_used = rdbuff_pos = 0;
	} else{
		rdbuff_pos += prevRead;
	}
	buff = getReadBuffer();
	if (prevRead) {
		if (buff.length == 0 && !eof) {
			int res = recv(sock,rdbuff,bufferSize,MSG_DONTWAIT);
			if (res == -1) {
				int e = errno;
				if (e != EWOULDBLOCK) throw SystemException(errno);
			} else if (res == 0) {
				eof = true;
			} else {
				rdbuff_used = res;
				rdbuff_pos = 0;
				buff = getReadBuffer();
			}
		}
		return buff;
	} else {
		if (buff.length == 0) {
			if (!eof) {
				int res = recv(sock, rdbuff,bufferSize,MSG_DONTWAIT);
				while (res == -1) {
					int e = errno;
					if (e != EWOULDBLOCK) throw SystemException(errno);
					waitForData();
					res = recv(sock, rdbuff,bufferSize,MSG_DONTWAIT);
				}
				if (res == 0) {
					eof = true;
					eofReported = true;
				} else {
					rdbuff_used = res;
					rdbuff_pos = 0;
					buff = getReadBuffer();
				}
			} else {
				if (eofReported)
					throw EndOfStreamException();
				eofReported = true;
			}
		}
		return buff;
	}

}
*/
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
/*
void SocketConnection::writeData(const BinaryView& data) {
	if (data.length) {
		if (wrbuff_pos == 0 && data.length > bufferSize) {
			sendAll(data);
		} else {
			std::size_t remain = bufferSize - wrbuff_pos;
			std::size_t transmit = std::min(remain,data.length);
			std::memcpy(wrbuff+wrbuff_pos,data.data,transmit);
			wrbuff_pos += transmit;
			if (wrbuff_pos == bufferSize) {
				flush();
			}
			BinaryView newdata = data.substr(transmit);
			if (newdata.length) writeData(newdata);
		}
	} else {
		closeOutput();
	}
}

void SocketConnection::flush() {
	BinaryView data(wrbuff, wrbuff_pos);
	sendAll(data);
	wrbuff_pos = 0;
}

*/
void SocketConnection::closeOutput() {
	flush();
	shutdown(sock,SHUT_WR);
}



Connection Connection::connect(const NetAddr &addr, const ConnectParams &params) {
	BinaryView b = addr.toSockAddr();
	const struct sockaddr *sa = reinterpret_cast<const struct sockaddr *>(b.data);
	int s = socket(sa->sa_family, SOCK_STREAM, IPPROTO_TCP);
	if (s == -1) throw SystemException(errno);
	int nblock = 1;ioctl(s, FIONBIO, &nblock);
	int r = ::connect(s, sa, b.length);
	if (r) {
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
				e = EFAULT;
				socklen_t l = sizeof(e);
				getsockopt(s,SOL_SOCKET, SO_ERROR, &e, &l);
				if (e != 0) {
					::close(s);
					throw SystemException(e);
				}
			}

		} else {
			::close(s);
			throw SystemException(e);
		}
	}
	if (params.factory != nullptr) return params.factory(addr, s);
	else {
		PSocketConnection conn = new SocketConnection(s,infinity,addr);
		return Connection(PConnection::staticCast(conn));
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
		EPollAsync &async = dynamic_cast<EPollAsync &>(*cntr.getHandle());
		RefCntPtr<SocketConnection> me(this);
		async.asyncWait(EPollAsync::wfRead,sock,timeoutOverride?timeoutOverride:iotimeout,[me,cntr,callback,timeoutOverride](EPollAsync::EventType ev){
			switch (ev) {
			case EPollAsync::etError: try {
					checkSocketError(me->sock);
				} catch (...) {
					callback(asyncError,BinaryView(nullptr,0));
				}
				break;
			case EPollAsync::etTimeout:
				callback(asyncTimeout, BinaryView(nullptr,0));
				break;
			case EPollAsync::etReadEvent: try {
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
		EPollAsync &async = dynamic_cast<EPollAsync &>(*cntr.getHandle());
		async.asyncWait(EPollAsync::wfWrite,sock,timeoutOverride?timeoutOverride:iotimeout,
				[me,data,offset,cntr,callback,timeoutOverride] (EPollAsync::EventType ev) {
					switch (ev) {
					case EPollAsync::etError:
						try {
							checkSocketError(me->sock);
						} catch (...) {
							callback(asyncError, data);
						}
						break;
					case EPollAsync::etTimeout:
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

void SocketConnection::checkSocketError(int fd) {
		int e = EFAULT;
		socklen_t l = sizeof(e);
		getsockopt(fd,SOL_SOCKET, SO_ERROR, &e, &l);
		if (e) throw SystemException(e);
}


BinaryView SocketConnection::getReadBuffer() const {
	return BinaryView(rdbuff+rdbuff_pos, rdbuff_used - rdbuff_pos);
}

bool SocketConnection::cancelAsyncRead(AsyncControl cntr) {
	EPollAsync &async = dynamic_cast<EPollAsync &>(*cntr.getHandle());
	return async.cancelWait(EPollAsync::wfRead,sock);

}
bool SocketConnection::cancelAsyncWrite(AsyncControl cntr){
	EPollAsync &async = dynamic_cast<EPollAsync &>(*cntr.getHandle());
	return async.cancelWait(EPollAsync::wfWrite,sock);

}



} /* namespace simpleServer */
