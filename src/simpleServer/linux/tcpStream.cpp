#include <sys/socket.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include "tcpStream.h"
#include "async.h"

#include <poll.h>

#include "../exceptions.h"

namespace simpleServer {

BinaryView TCPStream::implRead(bool nonblock) {

	MutableBinaryView b(inputBuffer, inputBufferSize);
	return implRead(b,nonblock);

}

BinaryView TCPStream::implWrite(BinaryView buffer, bool nonblock) {
	do {
		int r = send(sck, buffer.data, buffer.length, MSG_DONTWAIT);
		if (r < 0) {
			int e = errno;
			if (e != EWOULDBLOCK && e != EINTR && e != EAGAIN)
				throw SystemException(e,__FUNCTION__);
			if (nonblock) return buffer;
			if (!implWaitForWrite(iotimeout)) {
				throw TimeoutException();
			}
		} else if (r == 0) {
			return eofConst;
		} else {
			return buffer.substr(r);
		}
	} while (true);
}


static bool doPoll(int sock, int events, int timeoutms) {
	struct pollfd pfd;
	pfd.events = events;
	pfd.fd = sock;
	pfd.revents = 0;
	do {
		int r = poll(&pfd,1,timeoutms);
		if (r < 0) {
			int e = errno;
			if (e != EINTR && e != EAGAIN)
				throw SystemException(e,__FUNCTION__);
		} else {
			return r > 0;
		}
	} while (true);

}

bool TCPStream::implWaitForRead(int timeoutms) {
	return doPoll(sck,POLLIN|POLLRDHUP, timeoutms);
}

bool TCPStream::implWaitForWrite(int timeoutms) {
	return doPoll(sck,POLLOUT, timeoutms);
}

void TCPStream::implCloseInput() {
	shutdown(sck, SHUT_RD);
}

void TCPStream::implCloseOutput() {
	shutdown(sck, SHUT_WR);
}

TCPStream::TCPStream(int sck, int iotimeout, const NetAddr& peer)
	:sck(sck),iotimeout(iotimeout),peer(peer)
{
}

BinaryView TCPStream::implRead(MutableBinaryView buffer, bool nonblock) {
	do {
		int r = recv(sck,buffer.data, buffer.length, MSG_DONTWAIT);
		if (r < 0) {
			int e = errno;
			if (e == ECONNRESET) {
				return eofConst;
			}
			if (e != EWOULDBLOCK && e != EINTR && e != EAGAIN)
				throw SystemException(e,__FUNCTION__);
			if (nonblock) return BinaryView();
			if (!implWaitForRead(iotimeout)) {
				throw TimeoutException();
			}
		} else if (r == 0) {
			return eofConst;
		} else {
			return BinaryView(buffer.data, r);
		}

	} while (true);
}


void TCPStream::implWrite(WrBuffer& curBuffer, bool nonblock) {
 if (curBuffer.wrpos == 0) {
	 curBuffer = WrBuffer(outputBuffer,outputBufferSize,0);
 } else {
	 BinaryView v = curBuffer.getView();
	 BinaryView w = implWrite(v, nonblock);
	 if (w.empty()) {
		 curBuffer = WrBuffer(outputBuffer,outputBufferSize,0);
	 } else if (curBuffer.remain()>16) {
		 curBuffer = WrBuffer(curBuffer.ptr+w.length, 0, curBuffer.size-w.length);
	 } else if (w.length != v.length){
		 copydata(curBuffer.ptr, w.data, w.length);
		 curBuffer.wrpos = w.length;
	 }
 }
}

void TCPStream::implReadAsync(const MutableBinaryView& buffer, const Callback& cb) {
	if (asyncProvider == nullptr) throw NoAsyncProviderException();
	RefCntPtr<TCPStream> me(this);

	Callback cbc(cb);
	MutableBinaryView b(buffer);

	auto fn = [me,cbc,b](AsyncState state) {
		if (state == asyncOK) {
			BinaryView r = me->implRead(b,true);
			if (isEof(r)) {
				cbc(asyncEOF,r);
			} else if (r.empty()) {
				me->implReadAsync(b,cbc);
			}else {
				cbc(state, r);
			}
		} else {
			cbc(state, BinaryView(0,0));
		}

	};

	asyncProvider->runAsync(AsyncResource(sck, POLLIN),iotimeout, fn);
}

void TCPStream::implWriteAsync(const BinaryView& data, const Callback& cb) {
	if (asyncProvider == nullptr) throw NoAsyncProviderException();
	RefCntPtr<TCPStream> me(this);

	Callback cbc( cb);
	BinaryView b(data);

	auto fn = [me,cbc, b](AsyncState state) {
		if (state == asyncOK) {
			BinaryView r = me->implWrite(b, true);
			if (r.length == b.length) {
				me->implWriteAsync(b, cbc);
			}else {
				cbc(state, r);
			}
		} else {
			cbc(state, BinaryView(0,0));
		}
	};

	asyncProvider->runAsync(AsyncResource(sck,POLLOUT),iotimeout,fn);
}

void TCPStream::implFlush() {
	//not implemented
}

TCPStream::~TCPStream() noexcept {
	if (sck) {
		try {
			flush(writeWholeBuffer);
		} catch (...) {
			//do not try flush on error socket
		}
		close(sck);
	}

}


int TCPStream::setIOTimeout(int iotimeoutms) {
	int ret = iotimeout;
	iotimeout = iotimeoutms;
	return ret;
}

void TCPStream::implReadAsync(const Callback& cb) {
	MutableBinaryView b(inputBuffer, inputBufferSize);
	implReadAsync(b,cb);
}
}

