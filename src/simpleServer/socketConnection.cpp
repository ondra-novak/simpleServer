/*
 * socketConnection.cpp
 *
 *  Created on: May 6, 2017
 *      Author: ondra
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

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

void SocketConnection::waitForData() {
	flush();
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

BinaryView SocketConnection::getReadBuffer() const {
	return BinaryView(rdbuff+rdbuff_pos, rdbuff_used - rdbuff_pos);
}



} /* namespace simpleServer */
