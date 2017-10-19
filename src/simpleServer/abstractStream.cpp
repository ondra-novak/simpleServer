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
BinaryView AbstractStream::write(const BinaryView &databuff, WriteMode wrmode ) {

	//attempt to write the buffer itself causes that directwrite is called
	if (databuff.data == wrBuff.ptr) {
		return databuff.substr(writeBuffer(databuff, wrmode));
	}
	const BinaryView *b = &databuff;
	BinaryView tmp;
	//if databuff contains more data then buffer
	if (databuff.length > wrBuff.size) {
		//if the buffer is not empty
		if (wrBuff.wrpos) {
			//put begin of the databuff to the buffer
			tmp = write(databuff, writeNonBlock);
			b = &tmp;
			//and flush the whole buffer.
			flush(wrmode);
			//in mode writeCanBlock - only one block is allowed, which could happen during flush
			if (wrmode == writeCanBlock) wrmode = writeNonBlock;
		}
		//if the buffer is empty
		if (wrBuff.wrpos == 0)
			//send the databuff (or rest) directly with current write mode
			return databuff.substr(writeBuffer(*b,wrmode));

		//otherwise, use buffering
	}
	return writeBuffered(*b, wrmode);

}

bool AbstractStream::canRunAsync() const {
	return asyncProvider != nullptr;
}

BinaryView AbstractStream::writeBuffered(const BinaryView &databuff, WriteMode wrmode ) {
	//depend on write mode
	switch (wrmode) {
	case writeAndFlush:
		//in this mode, whole buffer is written and then flush is called
		//so first, restart function with writeWholeBuffer mode
		writeBuffered(databuff, writeWholeBuffer);
		//then flush
		flush(writeAndFlush);
		//everything was written
		return BinaryView(0,0);
	case writeWholeBuffer: {
			//in this mode, writting is repeating until everything is written
			//write it in mode writeCanBlock
			BinaryView b = writeBuffered(databuff, writeCanBlock);
			//everything has been written - exit now
			if (b.empty()) return b;
			//otherwise, restart writing with rest of the buffer
			else return writeBuffered(databuff.substr(b.length), writeWholeBuffer);
		}break;
	case writeNonBlock:
	case writeCanBlock:{
			//if there is no buffer available
			if (wrBuff.remain() == 0) {
				//flush the buffer with specified write mode
				//in non-block mode, nothing can happed
				flush(wrmode);
			}
			//prepare arguments
			const void *start = databuff.data;
			std::size_t sz = std::min(databuff.length, wrBuff.remain());
			//put data to the buffer
			std::memcpy(wrBuff.ptr+wrBuff.wrpos, start, sz);
			//return unused part of databuff
			return databuff.substr(sz);
		}
	default: throw std::runtime_error("Invalid write mode - unreachable code");
	}


}



MutableBinaryView simpleServer::AbstractStream::noOutputMode() {
	throw std::runtime_error("Stream is read only (no writes are possible)");
}

BinaryView IGeneralStream::eofConst(StrViewA("!EOF",0));

bool simpleServer::IGeneralStream::isEof(const BinaryView& buffer) {
	return buffer.data == eofConst.data;
}

AsyncProvider simpleServer::AbstractStream::setAsyncProvider( AsyncProvider asyncProvider) {
	std::swap(this->asyncProvider, asyncProvider);
	return asyncProvider;
}


}


