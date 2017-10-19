#pragma once

#include "abstractStream.h"

namespace simpleServer {

	class LimitedStream: public AbstractStream {
	public:

		LimitedStream(Stream source, std::size_t readLimit, std::size_t writeLimit)
			:source(source), readLimit(readLimit), writeLimit(writeLimit) {}


		virtual int setIOTimeout(int timeoutms) {return source.setIOTimeout(timeoutms);}
		virtual BinaryView readBuffer(bool nonblock) {
			BinaryView b = source.read(nonblock);
			if (b.length > readLimit) {
				BinaryView r = b.substr(0, readLimit);
				BinaryView pb = b.substr(readLimit);
				readLimit = 0;
				source.putBack(pb);
				return r;
			} else {
				readLimit -= b.length;
				return b;
			}
		}
		virtual MutableBinaryView createOutputBuffer() {
			return source.getWriteBuffer((size_t)-1);
		}
		virtual std::size_t writeBuffer(BinaryView buffer, WriteMode wrmode) {
			if (buffer.data == wrBuff.ptr) {
				source.commitWriteBuffer(buffer.length);
				flush(wrmode);
			}
			std::size_t sz  = source.write(buffer.substr(0,writeLimit),wrmode);
			source.getWriteBuffer((size_t)-1);
			writeLimit-=sz;
			return sz;
		}
		virtual bool waitForRead(int timeoutms) {
			return source.waitForInput(timeoutms);
		}
		virtual bool waitForWrite(int timeoutms) {
			source.waitForOutput(timeoutms);
		}

		virtual void closeInput() {
			readLimit = 0;
			rdBuff = BinaryView(0,0);
			source.putBackEof();
		}
		virtual void closeOutput() {
			source.closeOutput();
		}

		virtual void flushOutput() {
			source.flush(writeAndFlush);
		}

		static Stream create(Stream sourceStream, std::size_t limit) {
			return new LimitedStream(sourceStream, limit);
		}

		virtual void readAsyncBuffer(const Callback &cb) {

		}
		virtual void writeAsyncBuffer(const Callback &cb, BinaryView data) {

		}


	protected:
		Stream source;
		std::size_t readLimit;
		std::size_t writeLimit;


	};

}
