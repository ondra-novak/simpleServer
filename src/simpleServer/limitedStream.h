#pragma once

#include "abstractStream.h"

namespace simpleServer {

	class LimitedStream: public AbstractStream {
	public:

		LimitedStream(Stream source, std::size_t readLimit, std::size_t writeLimit, unsigned char fillChar)
			:source(source), readLimit(readLimit), writeLimit(writeLimit),fillChar(fillChar) {}
		~LimitedStream() {
			source.commitWriteBuffer(wrBuff.wrpos);
			padding();
		}


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
			BinaryView b = source.write(buffer.substr(0,writeLimit),wrmode);
			std::size_t written = buffer.length - b.length;
			writeLimit-=written;
			return written;
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
			padding();
		}

		virtual void flushOutput() {
			source.flush(writeAndFlush);
		}

		static Stream create(Stream sourceStream, std::size_t readLimit, std::size_t writeLimit, unsigned char fillChar = 0) {
			return new LimitedStream(sourceStream, readLimit, writeLimit, fillChar);
		}

		virtual bool canRunAsync() const {
			return source.canRunAsync();
		}

		virtual void readAsyncBuffer(const Callback &cb) override {
			RefCntPtr<LimitedStream> me(this);
			Callback ccpy(cb);
			source.readASync([me, ccpy](AsyncState st, const BinaryView &b){
				if (b.length > me->readLimit) {
					BinaryView newb = b.substr(0,me->readLimit);
					me->source.putBack(b.substr(me->readLimit));
					me->readLimit = 0;
					ccpy(st, newb);
				} else {
					me->readLimit-=b.length;
					ccpy(st, b);
				}
			});

		}
		virtual void writeAsyncBuffer(const Callback &cb, BinaryView data) override {
			BinaryView ldata = data.substr(0,writeLimit);
			std::size_t delta = ldata.length;
			writeLimit -= delta;
			Callback ccpy(cb);
			RefCntPtr<LimitedStream> me(this);
			source.writeAsync(ldata, [me,ccpy,delta](AsyncState st, const BinaryView &remain) {
				if (st == asyncOK) {
					me->writeLimit += remain.length;
					ccpy(st,remain);
				} else {
					me->writeLimit += delta;
					ccpy(st,remain);
				}
			});
		}


	protected:
		Stream source;
		std::size_t readLimit;
		std::size_t writeLimit;
		unsigned char fillChar;

		void padding() {
			while (writeLimit) {
				source(fillChar);
				writeLimit--;
			}
		}

	};

}
