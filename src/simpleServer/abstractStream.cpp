#include "abstractStream.h"

#include <cstring>


namespace simpleServer {

///Writes block of bytes
/**
 * @param buffer buffer to write
 * @param wrmode specify write mode
 *
 * Function uses buffer to collect small writes into single brust write.
 */
BinaryView AbstractStream::write(BinaryView buffer, WriteMode wrmode ) {
	bool flushAtEnd;
	if (buffer.length > wrBuff.size) {
		flush(wrmode);
		if (wrBuff.wrpos == 0)
			return buffer.substr(writeBuffer(buffer,wrmode));
	}

	if (wrBuff.size == 0) wrBuff = createOutputBuffer();

	switch (wrmode) {
	case writeAndFlush:
		for(;;) {
			auto remain = wrBuff.remain();
			BinaryView part = buffer.substr(0,remain);
			std::memcpy(wrBuff.ptr+wrBuff.wrpos, part.data, part.length);
			buffer = buffer.substr(part.length);
			if (buffer.empty()) {
				flush(writeAndFlush);
				break;
			} else {
				flush(writeCanBlock);
			}
		}
		return buffer;
	case writeWholeBuffer:
		for(;;) {
			auto remain = wrBuff.remain();
			BinaryView part = buffer.substr(0,remain);
			std::memcpy(wrBuff.ptr+wrBuff.wrpos, part.data, part.length);
			buffer = buffer.substr(part.length);
			if (buffer.empty()) {
				break;
			} else {
				flush(writeCanBlock);
			}
		}
		return buffer;
	case writeNonBlock:
	case writeCanBlock:{
			auto remain = wrBuff.remain();
			if (remain == 0) {
				flush(wrmode);
				remain = wrBuff.remain();
			}
			BinaryView part = buffer.substr(0,remain);
			std::memcpy(wrBuff.ptr+wrBuff.wrpos, part.data, part.length);
			buffer = buffer.substr(part.length);
		}return buffer;

	}
}



}

