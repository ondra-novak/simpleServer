#include <unistd.h>
#include "fileStream.h"
#include "async.h"


#include "../exceptions.h"

namespace simpleServer {

BinaryView FileStream::implRead(bool nonblock) {

	MutableBinaryView b(inputBuffer, inputBufferSize);
	return implRead(b,nonblock);

}

BinaryView FileStream::implWrite(BinaryView buffer, bool nonblock) {
	do {
		int r = ::write(fd, buffer.data, buffer.length);
		if (r < 0) {
			int e = errno;
			if (e == EPIPE) {
				return eofConst;
			}
			throw SystemException(e,__FUNCTION__);
		} else if (r == 0) {
			return eofConst;
		} else {
			return buffer.substr(r);
		}
	} while (true);
}



bool FileStream::implWaitForRead(int timeoutms) {
	return true;
}

bool FileStream::implWaitForWrite(int timeoutms) {
	return true;
}

void FileStream::implCloseInput() {

}

void FileStream::implCloseOutput() {

}


FileStream::FileStream(int fd)
	:fd(fd)
{


}

BinaryView FileStream::implRead(MutableBinaryView buffer, bool nonblock) {
	do {
		int r = ::read(fd,buffer.data, buffer.length);
		if (r < 0) {
			int e = errno;
			throw SystemException(e,__FUNCTION__);
		} else if (r == 0) {
			return eofConst;
		} else {
			return BinaryView(buffer.data, r);
		}
	} while (true);
}


bool FileStream::implWrite(WrBuffer& curBuffer, bool nonblock) {
 if (curBuffer.wrpos == 0) {
	 curBuffer = WrBuffer(outputBuffer,outputBufferSize,0);
 } else {
	 BinaryView v = curBuffer.getView();
	 BinaryView w = implWrite(v, nonblock);
	 if (isEof(w)) return false;
	 if (w.empty()) {
		 curBuffer = WrBuffer(outputBuffer,outputBufferSize,0);
	 } else if (curBuffer.remain()>16) {
		 curBuffer = WrBuffer(curBuffer.ptr+w.length, 0, curBuffer.size-w.length);
	 } else if (w.length != v.length){
		 copydata(curBuffer.ptr, w.data, w.length);
		 curBuffer.wrpos = w.length;
	 }
 }
 return true;
}


void FileStream::implReadAsync(const MutableBinaryView& buffer, const Callback& cb) {
	cb(asyncOK, implRead(buffer,false));
}

void FileStream::implWriteAsync(const BinaryView& data, const Callback& cb) {
	cb(asyncOK, implWrite(data,false));

}

bool FileStream::implFlush() {
	::syncfs(fd);
	return true;
}

FileStream::~FileStream() noexcept {
	if (fd) {
		try {
			flush(writeWholeBuffer);
		} catch (...) {
			//do not try flush on error socket
		}
		close(fd);
	}

}


int FileStream::setIOTimeout(int) {
	return 0;
}

void FileStream::implReadAsync(const Callback& cb) {
	MutableBinaryView b(inputBuffer, inputBufferSize);
	implReadAsync(b,cb);
}
}

