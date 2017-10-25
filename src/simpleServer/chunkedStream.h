#pragma once
#include "abstractStream.h"


namespace simpleServer {

template<std::size_t chunkSize=4096>
class ChunkedStream: public AbstractStream {
public:
	ChunkedStream(const Stream &source):source(source)
	{
		setAsyncProvider(source.getAsyncProvider());
	}
	~ChunkedStream() {
		if (wrBuff.size)
			closeOutput();

	}


	virtual int setIOTimeout(int timeoutms) {
		return source.setIOTimeout(timeoutms);
	}

	virtual BinaryView readBuffer(bool nonblock);
	virtual MutableBinaryView createOutputBuffer();
	virtual std::size_t writeBuffer(BinaryView buffer, WriteMode wrmode);
	virtual bool waitForRead(int timeoutms);
	virtual bool waitForWrite(int timeoutms);
	virtual void closeInput();
	virtual void closeOutput();
	virtual void flushOutput();

	virtual void readAsyncBuffer(const Callback &cb) override;
	virtual void writeAsyncBuffer(const Callback &cb, BinaryView data) override;


	static Stream create(const Stream &source) {
		return new ChunkedStream(source);
	}

protected:
	Stream source;


	//readPart
	enum ReadState {
		chunkFinish,
		chunkSizeNumber,
		chunkBegin,
		chunkData,
		chunkLast1,
		chunkLast2,
		chunkEof
	};

	ReadState curState = chunkSizeNumber;
	std::size_t curChunkSize = 0;


	void invalidChunk();

	//writePart

	static const std::size_t chunkBufferSize = chunkSize;
	static const std::size_t chunkBufferDataStart = 10;


	unsigned char chunkBuffer[chunkBufferSize+2+10]; //2 bytes for CRLF //10 bytes for first line
	bool outputClosed = false;


};

template<std::size_t chunkSize>
BinaryView ChunkedStream<chunkSize>::readBuffer(bool nonblock) {

	if (curState == chunkEof) return eofConst;
	BinaryView d = source.read(nonblock);
	while (!d.empty()) {

		unsigned char c = d[0];

		switch(curState) {
		case chunkFinish:
			if (c != '\n') invalidChunk();
			curState = chunkSizeNumber;
			break;
		case chunkSizeNumber:
			c = toupper(c);
			if (isdigit(c)) {
				curChunkSize = curChunkSize * 16 + (c - '0');
			} else if (c >= 'A' && c<='F') {
				curChunkSize = curChunkSize * 16 + (c - 'A' + 10);
			} else if (c == '\r') {
				curState = chunkBegin;
			} else {
				invalidChunk();
			}
			break;
		case chunkBegin:
			if (c != '\n') invalidChunk();
			if (curChunkSize == 0) {
				curState = chunkLast1;
			} else {
				curState = chunkData;
			}
			break;
		case chunkData:
			if (curChunkSize == 0) {
				if (c != '\r') invalidChunk();
				curState = chunkFinish;
			} else {
				BinaryView data = d.substr(0, curChunkSize);
				BinaryView pb = d.substr(curChunkSize);
				putBack(pb);
				curChunkSize -= data.length;
				return data;
			}
			break;
		case chunkLast1:
			if (c != '\r') invalidChunk();
			curState = chunkLast2;
			break;
		case chunkLast2:
			if (c != '\n') invalidChunk();
			curState = chunkEof;
			putBack(d.substr(1));
			return eofConst;
			break;
		}
		d = d.substr(1);
		if (d.empty()) {
			d = source.read(nonblock);
		}
	};
	return d;
}

template<std::size_t chunkSize>
MutableBinaryView ChunkedStream<chunkSize>::createOutputBuffer() {
	return MutableBinaryView(chunkBuffer+chunkBufferDataStart, chunkBufferSize);
}


template<std::size_t chunkSize>
std::size_t ChunkedStream<chunkSize>::writeBuffer(BinaryView buffer, WriteMode wrmode) {

	static unsigned char hexChar[]="0123456789abcdef";


	//we will not accept empty buffer
	if (buffer.empty()) return 0;
	//this function can be called with any buffer, only chunkBuffer can be used here
	//so we need to split function into two cases
	//the argument is chunkBuffer = this means that AbstractStream is trying to free some space in the buffer
	//the argument is other buffer = this means that we must put data to the chunkBuffer
	if (isMyBuffer(buffer)) {
		//write own buffer

		//chunk cannot be written in non-blocking mode (it will be writen during wait)
		if (wrmode == writeNonBlock) return 0;

		int wrpos = chunkBufferDataStart;
		//write \r\n before the payload
		chunkBuffer[--wrpos] = '\n';
		chunkBuffer[--wrpos] = '\r';
		//write the length before payload
		std::size_t sz = buffer.length;
		//fortunately we can write number from back to front
		while (sz != 0) {
			auto mod = sz & 0xF;
			sz >>= 4;
			chunkBuffer[--wrpos] = hexChar[mod];
		}
		//put '\r' after payload
		//put '\n' after payload
		chunkBuffer[buffer.length+chunkBufferDataStart] = '\r';
		chunkBuffer[buffer.length+chunkBufferDataStart+1] = '\n';
		//create transfer state
		BinaryView chunk(chunkBuffer+wrpos, chunkBufferDataStart-wrpos+buffer.length+2);
		//try to write buffer in current mode
		source.write(chunk, wrmode==writeAndFlush?wrmode:writeWholeBuffer);

		return buffer.length;

	} else {

		switch (wrmode) {
		case writeNonBlock:
		case writeCanBlock:
			return buffer.length - write(buffer.substr(0,chunkBufferSize), wrmode).length;
			break;
		case writeWholeBuffer:
		case writeAndFlush:
			while (!buffer.empty()) {
				BinaryView x = write(buffer.substr(0,chunkBufferSize), writeCanBlock);
				buffer = buffer.substr(chunkBufferSize-x.length);
			}
			if (wrmode == writeAndFlush) {
				flush(wrmode);
			}
			return buffer.length;
		}
	}
}

template<std::size_t chunkSize>
bool ChunkedStream<chunkSize>::waitForRead(int timeoutms) {
	return source.waitForInput(timeoutms);
}

template<std::size_t chunkSize>
bool ChunkedStream<chunkSize>::waitForWrite(int timeoutms) {
	bool r = source.waitForOutput(timeoutms);
	//we are ready to write and there is full buffer,
	if (r && wrBuff.remain() == 0) {
		//flush the buffer now with writeWholeBuffer
		flush(writeWholeBuffer);
		//repeat waiting
		r = source.waitForOutput(timeoutms);
	}
	return r;

}

template<std::size_t chunkSize>
void ChunkedStream<chunkSize>::closeInput() {
	curState = chunkEof;
	source.putBackEof();
}

template<std::size_t chunkSize>
void ChunkedStream<chunkSize>::closeOutput() {
	if (!outputClosed) {
		flush(writeWholeBuffer);
		BinaryView endChunk(StrViewA("0\r\n\r\n"));
		source.write(endChunk,writeWholeBuffer);
		outputClosed = true;
	}
}

template<std::size_t chunkSize>
void ChunkedStream<chunkSize>::flushOutput() {
	source.flush(writeWholeBuffer);
}

template<std::size_t chunkSize>
inline void ChunkedStream<chunkSize>::readAsyncBuffer(const Callback& cb) {
	if (asyncProvider == nullptr) throw NoAsyncProviderException();
	BinaryView rd = source.read(true);
	if (isEof(rd)) {
		fakeAsync(asyncEOF, eofConst, cb);
	} else if (rd.empty()) {
		Callback ccb = cb;
		RefCntPtr<ChunkedStream> me(this);
		source.readASync([=](AsyncState st, const BinaryView &data){
			if (st == asyncOK) {
				putBack(data);
				BinaryView rd = source.read(true);
				if (isEof(rd)) ccb(asyncEOF, BinaryView(0,0));
				else if (rd.empty()) me->readAsyncBuffer(ccb);
				else ccb(asyncOK, rd);
			} else {
				ccb(st,data);
			}
		});
	} else {
		fakeAsync(asyncOK, rd, cb);
	}
}

template<std::size_t chunkSize>
inline void ChunkedStream<chunkSize>::writeAsyncBuffer(const Callback& cb, BinaryView data) {
}

template<std::size_t chunkSize>
void ChunkedStream<chunkSize>::invalidChunk() {
	throw std::runtime_error("Invalid chunk");
}

} /* namespace simpleServer */


