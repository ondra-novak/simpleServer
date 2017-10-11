#pragma once

#include "abstractStream.h"

namespace simpleServer {

	class LimitedStream: public AbstractStream {
	public:

		LimitedStream(Stream source, std::size_t readLimit):source(source), readLimit(readLimit) {}


		virtual int setIOTimeout(int timeoutms) {return source.setIOTimeout();}
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
			return noOutputMode();
		}
		virtual std::size_t writeBuffer(BinaryView buffer, WriteMode wrmode) {
			noOutputMode();throw;
		}
		virtual bool waitForRead(int timeoutms) {
			return source.waitForInput(timeoutms);
		}
		virtual bool waitForWrite(int timeoutms) {
			noOutputMode();throw;
		}

		virtual void closeInput() {
			readLimit = 0;
			rdBuff = BinaryView(0,0);
			source.putBackEof();
		}
		virtual void closeOutput() {
			noOutputMode();throw;
		}


		virtual void flushOutput() {
			noOutputMode();
		}

		Stream create(Stream sourceStream, std::size_t limit) {
			return new LimitedStream(sourceStream, limit);
		}

	protected:
		Stream source;
		std::size_t readLimit;


	};

}
