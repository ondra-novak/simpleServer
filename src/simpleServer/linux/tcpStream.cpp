#include <sys/socket.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include "tcpStream.h"

#include <poll.h>

#include "../exceptions.h"

namespace simpleServer {

BinaryView TCPStream::readBuffer(bool nonblock) {
}

MutableBinaryView TCPStream::createOutputBuffer() {

	return MutableBinaryView(reinterpret_cast<unsigned char *>(outputBuffer), outputBufferSize);

}

std::size_t TCPStream::writeBuffer(BinaryView buffer, WriteMode wrmode) {
	switch (wrmode) {
	case writeAndFlush:
	case writeWholeBuffer: {
		std::size_t wrt = 0;
		do {
			std::size_t sz = writeBuffer(buffer, writeCanBlock);
			wrt+=sz;
			buffer = buffer.substr(sz);
		} while (!buffer.empty());
		return wrt;
	}
	case writeCanBlock: {
		std::size_t sz = writeBuffer(buffer, writeNonBlock);
		while (sz == 0) {
			waitForWrite(iotimeout);
			sz = writeBuffer(buffer, writeNonBlock);
		}
		return sz;
	}
	case writeNonBlock: {
		int r = send(sck, buffer.data,buffer.length,MSG_DONTWAIT);
		if (r < 0) {
			int e = errno;
			if (e == EWOULDBLOCK || e == EINTR || e == EAGAIN) return 0;
			else throw SystemException(e,__FUNCTION__);
		} else {
			return r;
		}
	}
	}
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

bool TCPStream::waitForRead(int timeoutms) {
	return doPoll(sck,POLLIN|POLLRDHUP, timeoutms);
}

bool TCPStream::waitForWrite(int timeoutms) {
	return doPoll(sck,POLLOUT, timeoutms);
}

void TCPStream::closeInput() {
	shutdown(sck, SHUT_RD);
}

void TCPStream::closeOutput() {
	shutdown(sck, SHUT_WR);
}

void TCPStream::flushOutput() {
	//not necesery
}

}

