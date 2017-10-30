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

	~ChunkedStream() noexcept {}


	virtual int setIOTimeout(int timeoutms) {
		return source.setIOTimeout(timeoutms);
	}
protected:

	template<typename T> friend class RefCntPtr;

	virtual void onRelease() {
		if (wrBuff.size)
			implCloseOutput();
		source = nullptr;
		AbstractStream::onRelease();
	}


	virtual BinaryView implRead(bool nonblock) override;
	virtual BinaryView implRead(MutableBinaryView buffer, bool nonblock) override;
	virtual BinaryView implWrite(BinaryView buffer, bool nonblock) override;
	virtual void implWrite(WrBuffer &curBuffer, bool nonblock) override;

	virtual void implReadAsync(const Callback &cb) override;
	virtual void implReadAsync(const MutableBinaryView &buffer, const Callback &cb) override ;
	virtual void implWriteAsync(const BinaryView &data, const Callback &cb) override;

	virtual bool implWaitForRead(int timeoutms) override;
	virtual bool implWaitForWrite(int timeoutms) override;
	virtual void implCloseInput() override;
	virtual void implCloseOutput() override;
	virtual void implFlush() override;



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




	unsigned char hdrBuffer[50];
	unsigned char chunkBuffer[chunkSize]; //2 bytes for CRLF //10 bytes for first line
	bool outputClosed = false;

	static unsigned char *writeHex(std::size_t sz, unsigned char *x) {
		if (sz) {
			unsigned char *k = writeHex(sz>>4,x);
			static unsigned char digits[]="0123456789abcdef";
			*k = digits[sz & 0xf];
			return k+1;
		} else {
			return x;
		}
	}

	BinaryView makeHdr(std::size_t sz) {
		unsigned char *e = writeHex(sz, hdrBuffer);
		e[0] = '\r';
		e[1] = '\n';
		return BinaryView(hdrBuffer, (e-hdrBuffer)+2);
	}

};

template<std::size_t chunkSize>
BinaryView ChunkedStream<chunkSize>::implRead(bool nonblock) {

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
bool ChunkedStream<chunkSize>::implWaitForRead(int timeoutms) {
	return source.waitForInput(timeoutms);
}

template<std::size_t chunkSize>
bool ChunkedStream<chunkSize>::implWaitForWrite(int timeoutms) {
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
void ChunkedStream<chunkSize>::implCloseInput() {
	curState = chunkEof;
	source.putBackEof();
}

template<std::size_t chunkSize>
void ChunkedStream<chunkSize>::implCloseOutput() {
	if (!outputClosed) {
		flush(writeWholeBuffer);
		BinaryView endChunk(StrViewA("0\r\n\r\n"));
		source.write(endChunk,writeWholeBuffer);
		outputClosed = true;
		wrBuff.size = 0;
		wrBuff.wrpos = 0;
	}
}

template<std::size_t chunkSize>
void ChunkedStream<chunkSize>::implFlush() {
	source.flush(writeWholeBuffer);
}

template<std::size_t chunkSize>
inline void ChunkedStream<chunkSize>::implReadAsync(const Callback& cb) {
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
				else if (rd.empty()) me->implReadAsync(ccb);
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
inline BinaryView ChunkedStream<chunkSize>::implRead(MutableBinaryView buffer,bool nonblock) {

	BinaryView k = implRead(nonblock);
	if (k.empty()) return k;
	BinaryView rm = k.substr(buffer.length);
	k = k.substr(0,buffer.length);
	copydata(buffer.data, k.data, k.length);
	putBack(rm);
	return BinaryView(buffer.data, k.length);

}

template<std::size_t chunkSize>
inline BinaryView ChunkedStream<chunkSize>::implWrite(BinaryView buffer, bool nonblock) {
	source.write(makeHdr(buffer.length),writeWholeBuffer);
	source.write(buffer,writeWholeBuffer);
	source.write(BinaryView(StrViewA("\r\n")),writeWholeBuffer);
	return BinaryView(0,0);
}
template<std::size_t chunkSize>
inline void ChunkedStream<chunkSize>::implWrite(WrBuffer& curBuffer, bool nonblock) {
	if (curBuffer.size == 0) {
		curBuffer = WrBuffer(chunkBuffer,chunkSize);
	} else {
		//we know, that function is always write whole buffer
		implWrite(curBuffer.getView(), false);
		curBuffer.wrpos = 0;
	}
}

template<std::size_t chunkSize>
inline void ChunkedStream<chunkSize>::implWriteAsync(const BinaryView &data, const Callback& cb) {
	RefCntPtr<ChunkedStream> me(this);
	Callback ccb(cb);

	source.writeAsync(makeHdr(data.length),[=](AsyncState st, const BinaryView &r){
		if (st == asyncOK) {
			me->source.writeAsync(data,[=](AsyncState st, const BinaryView &r){
				if (st == asyncOK) {
					me->source.writeAsync(BinaryView(StrViewA("\r\n")),ccb);
				} else{
					ccb(st,r);
				}
			},true);
		} else {
			ccb(st,r);
		}
	},true);
}




template<std::size_t chunkSize>
inline void ChunkedStream<chunkSize>::implReadAsync(const MutableBinaryView& buffer,const Callback& cb) {
	Callback ccb(cb);
	RefCntPtr<ChunkedStream> me(this);
	MutableBinaryView b(buffer);
	readAsync([=](AsyncState st, const BinaryView &data) {
		if (st == asyncOK) {

			BinaryView p = data.substr(0,b.length);
			copydata(buffer.data, p.data, p.length);
			me->putBack(data.substr(b.length));
			ccb(st,b);
		}
		else {
			ccb(st,data);
		}
	});

}


template<std::size_t chunkSize>
void ChunkedStream<chunkSize>::invalidChunk() {
	throw std::runtime_error("Invalid chunk");
}

} /* namespace simpleServer */


